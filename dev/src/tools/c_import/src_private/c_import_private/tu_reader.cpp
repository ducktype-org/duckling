#include "clang_raii.hpp"

#include <c_import/identifiers.hpp>
#include <c_import/tu_reader.hpp>
#include <c_import/type_mapper.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <exception>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace c_import {

	using detail::OwnedIndex;
	using detail::OwnedString;
	using detail::OwnedTranslationUnit;
	using detail::spellingOf;

	namespace {

		/** A record definition found anywhere in the translation unit, nested ones included. */
		struct DiscoveredRecord final {
			CXCursor    cursor;
			std::string name;
			/** Non-empty when the record cannot be translated. */
			std::string rejection;
			/** Records named by value, which carry their rejection over to this one. */
			std::vector<std::string> by_value_deps;
		};

		struct Context final {
			TranslationUnitModel model;
			NameRegistry         registry;
			/** Every record definition, in discovery order. */
			std::vector<DiscoveredRecord> records;
			/**
			 * Records that actually get a class. Decided before anything is emitted, so a type
			 * can never name a class that is missing or that is skipped later in the file.
			 */
			std::unordered_set<std::string> emitted_records;
			/**
			 * Set while records are being discovered, when it is not yet known which ones get a
			 * class. Record references are then assumed fine and settled by the fixpoint, so
			 * everything else about a field type is still checked.
			 */
			bool discovering = false;
			/** USR of an anonymous record to the name chosen for it. */
			std::unordered_map<std::string, std::string> anonymous_names;
			std::uint32_t                                anonymous_counter = 0;
		};

		std::string usrOf(CXCursor cursor) { return OwnedString(clang_getCursorUSR(cursor)).str(); }

		/** Module name for the header a cursor came from: its stem, scrubbed of separators. */
		std::string originOf(CXCursor cursor) {
			CXFile       file   = nullptr;
			unsigned int line   = 0;
			unsigned int column = 0;
			unsigned int offset = 0;
			clang_getFileLocation(clang_getCursorLocation(cursor), &file, &line, &column, &offset);
			if (file == nullptr) return {};

			const std::string path = OwnedString(clang_getFileName(file)).str();

			const std::size_t slash = path.find_last_of("/\\");
			std::string       stem  = slash == std::string::npos ? path : path.substr(slash + 1);

			const std::size_t dot = stem.find_last_of('.');
			if (dot != std::string::npos && dot != 0) stem = stem.substr(0, dot);

			for (char& character: stem)
				if (std::isalnum(static_cast<unsigned char>(character)) == 0) character = '_';

			return stem;
		}

		bool isCharKind(CXTypeKind kind) {
			return kind == CXType_Char_S || kind == CXType_Char_U || kind == CXType_SChar
			    || kind == CXType_UChar;
		}

		CType scalarFromInteger(CXType type, bool is_signed) {
			switch (clang_Type_getSizeOf(type)) {
			case 1:
				return makeScalar(is_signed ? Scalar::Int8 : Scalar::UInt8);
			case 2:
				return makeScalar(is_signed ? Scalar::Int16 : Scalar::UInt16);
			case 4:
				return makeScalar(is_signed ? Scalar::Int32 : Scalar::UInt32);
			case 8:
				return makeScalar(is_signed ? Scalar::Int64 : Scalar::UInt64);
			default:
				return makeUnsupported("integer width is not 8, 16, 32 or 64 bits");
			}
		}

		std::string recordNameOf(Context& context, CXType type) {
			CXCursor    declaration = clang_getTypeDeclaration(type);
			const bool  is_union    = declaration.kind == CXCursor_UnionDecl;
			std::string tag         = spellingOf(declaration);

			if (!tag.empty() && clang_Cursor_isAnonymous(declaration) == 0)
				return is_union ? taggedUnionName(tag) : taggedRecordName(tag);

			const std::string usr = usrOf(declaration);
			if (auto found = context.anonymous_names.find(usr);
			    found != context.anonymous_names.end())
				return found->second;

			std::string name = "__anon_record_" + std::to_string(context.anonymous_counter++);
			context.anonymous_names.emplace(usr, name);
			return name;
		}

		CType mapType(Context& context, CXType type);

		CType mapPointee(Context& context, CXType pointee) {
			const CXType canonical = clang_getCanonicalType(pointee);

			// `char *` keeps the ergonomic spelling: the pointee is one byte either way and no
			// argument promotion applies to it.
			if (isCharKind(canonical.kind)) return makeScalar(Scalar::Char);

			if (canonical.kind == CXType_Void) return makeUnsupported("void");

			return mapType(context, canonical);
		}

		CType mapType(Context& context, CXType type) {
			const CXType canonical = clang_getCanonicalType(type);

			switch (canonical.kind) {
			case CXType_Bool:
				return makeScalar(Scalar::Bool);

			case CXType_Char_S:
			case CXType_SChar:
				return makeScalar(Scalar::Int8);
			case CXType_Char_U:
			case CXType_UChar:
				return makeScalar(Scalar::UInt8);

			case CXType_Short:
			case CXType_Int:
			case CXType_Long:
			case CXType_LongLong:
				return scalarFromInteger(canonical, true);

			case CXType_UShort:
			case CXType_UInt:
			case CXType_ULong:
			case CXType_ULongLong:
				return scalarFromInteger(canonical, false);

			case CXType_Int128:
			case CXType_UInt128:
				return makeUnsupported("128-bit integers have no C ABI mapping");

			case CXType_Float:
				return makeScalar(Scalar::Float32);
			case CXType_Double:
				return makeScalar(Scalar::Float64);
			case CXType_LongDouble:
				return makeUnsupported("`long double` is not supported yet");

			case CXType_Enum:
				return mapType(
					context, clang_getEnumDeclIntegerType(clang_getTypeDeclaration(canonical))
				);

			case CXType_Pointer: {
				const CXType pointee = clang_getPointeeType(canonical);
				// A function pointer is an opaque address: Duckling has no C callback type.
				if (clang_getCanonicalType(pointee).kind == CXType_FunctionProto
				    || clang_getCanonicalType(pointee).kind == CXType_FunctionNoProto)
					return makeOpaquePointer();

				// A pointer to something that gets no class is still a usable address.
				CType mapped = mapPointee(context, pointee);
				if (!isSupported(mapped)) return makeOpaquePointer();

				return makePointer(std::move(mapped));
			}

			case CXType_Record: {
				std::string name = recordNameOf(context, canonical);

				// Naming a record that gets no class would dangle. That covers opaque records,
				// records skipped for their contents, and ones that are never emitted at all
				// such as nested definitions or clang's implicit builtins. A pointer to any of
				// them degrades to `cptr u8` through the caller's isSupported check.
				if (!context.discovering && !context.emitted_records.contains(name))
					return makeUnsupported("`" + name + "` has no generated class");

				return makeRecord(std::move(name));
			}

			case CXType_ConstantArray: {
				const long long count = clang_getArraySize(canonical);
				if (count <= 0) return makeUnsupported("zero-length array");
				return makeArray(
					mapType(context, clang_getArrayElementType(canonical)),
					static_cast<std::uint64_t>(count)
				);
			}

			case CXType_IncompleteArray:
				return makeUnsupported("flexible array member");

			case CXType_Void:
				return makeUnsupported("void");

			default:
				return makeUnsupported(
					"no C ABI mapping for `" + detail::spellingOf(canonical) + "`"
				);
			}
		}

		void skip(Context& context, std::string kind, std::string name, std::string reason) {
			context.model.decls.emplace_back(CSkipped{
				.kind = std::move(kind), .name = std::move(name), .reason = std::move(reason) });
		}

		/**
		 * @brief Makes a C field name usable as a Duckling identifier.
		 *
		 * Unlike a function name, a field name is never linked - the C ABI places fields by
		 * position - so a name that collides with a keyword can be renamed instead of costing
		 * the whole record. Trailing underscores are added until the name is free.
		 */
		std::string usableFieldName(
			std::string name, const std::vector<CField>& taken, std::size_t index
		) {
			if (name.empty()) name = "field" + std::to_string(index);

			const auto is_taken = [&taken](const std::string& candidate) {
				return std::ranges::any_of(taken, [&candidate](const CField& field) {
					return field.name == candidate;
				});
			};

			while (isDucklingKeyword(name) || is_taken(name)) name.push_back('_');

			return name;
		}

		/** Records named by value by @p cursor's fields, plus any local reason to reject it. */
		void inspectRecord(Context& context, DiscoveredRecord& record) {
			if (record.cursor.kind == CXCursor_UnionDecl) {
				record.rejection = "unions have no C ABI mapping";
				return;
			}

			struct Inspector final {
				Context*          context   = nullptr;
				DiscoveredRecord* record    = nullptr;
				bool              has_field = false;
			};

			Inspector inspector{ .context = &context, .record = &record };

			clang_visitChildren(
				record.cursor,
				[](CXCursor child, CXCursor, CXClientData data) {
					auto& [inspected_context, inspected, has_field]
						= *static_cast<Inspector*>(data);

					// libclang reports an anonymous member as a nested record declaration rather
				    // than a field. Duckling has no anonymous members, so dropping it would
				    // silently change the record's size and field offsets.
					if ((child.kind == CXCursor_StructDecl || child.kind == CXCursor_UnionDecl)
				        && clang_Cursor_isAnonymousRecordDecl(child) != 0) {
						if (inspected->rejection.empty())
							inspected->rejection = "contains an anonymous struct or union member";
						return CXChildVisit_Continue;
					}

					if (child.kind != CXCursor_FieldDecl) return CXChildVisit_Continue;

					has_field = true;

					if (clang_Cursor_isBitField(child) != 0) {
						if (inspected->rejection.empty())
							inspected->rejection
								= "contains the bitfield `" + spellingOf(child) + "`";
						return CXChildVisit_Continue;
					}

					const CType mapped = mapType(*inspected_context, clang_getCursorType(child));
					if (!isSupported(mapped)) {
						if (inspected->rejection.empty())
							inspected->rejection
								= "field `" + spellingOf(child)
						        + "` is not C ABI compatible: " + unsupportedReason(mapped);
						return CXChildVisit_Continue;
					}

					CXType field_type = clang_getCanonicalType(clang_getCursorType(child));

					// An array of records carries the same by-value dependency as the record.
					while (field_type.kind == CXType_ConstantArray)
						field_type = clang_getCanonicalType(clang_getArrayElementType(field_type));

					if (field_type.kind == CXType_Record)
						inspected->by_value_deps.push_back(
							recordNameOf(*inspected_context, field_type)
						);

					return CXChildVisit_Continue;
				},
				&inspector
			);

			// An `extern("C")` class with no fields is rejected by the compiler.
			if (record.rejection.empty() && !inspector.has_field)
				record.rejection = "has no fields";
		}

		/**
		 * @brief Finds every record definition in the translation unit and decides which ones
		 *        can be emitted, before anything is written.
		 *
		 * Doing this up front is what lets a type reference be trusted: nested and anonymous
		 * definitions are found (the main visitor only walks top-level cursors), and a record
		 * skipped later in the file is already known to be skipped when an earlier declaration
		 * points at it.
		 */
		void discoverRecords(Context& context, CXCursor translation_unit) {
			clang_visitChildren(
				translation_unit,
				[](CXCursor cursor, CXCursor, CXClientData data) {
					auto& discovered = *static_cast<Context*>(data);

					if ((cursor.kind == CXCursor_StructDecl || cursor.kind == CXCursor_UnionDecl)
				        && clang_isCursorDefinition(cursor) != 0) {
						std::string name = recordNameOf(discovered, clang_getCursorType(cursor));
						if (!std::ranges::any_of(
								discovered.records,
								[&name](const DiscoveredRecord& seen) { return seen.name == name; }
							))
							discovered.records.push_back(DiscoveredRecord{ .cursor = cursor,
						                                                   .name = std::move(name),
						                                                   .rejection     = {},
						                                                   .by_value_deps = {} });
					}

					return CXChildVisit_Recurse;
				},
				&context
			);

			context.discovering = true;
			for (auto& record: context.records) inspectRecord(context, record);
			context.discovering = false;

			// A record is unsupported when anything it holds by value is, so the rejection has
			// to be propagated to a fixpoint.
			bool changed = true;
			while (changed) {
				changed = false;
				for (auto& record: context.records) {
					if (!record.rejection.empty()) continue;

					for (const auto& dependency: record.by_value_deps) {
						const auto found = std::ranges::find_if(
							context.records,
							[&dependency](const DiscoveredRecord& other) {
								return other.name == dependency;
							}
						);

						const bool dependency_rejected
							= found == context.records.end() || !found->rejection.empty();

						if (dependency_rejected) {
							record.rejection = "holds `" + dependency + "`, which was skipped";
							changed          = true;
							break;
						}
					}
				}
			}

			for (const auto& record: context.records) {
				if (!record.rejection.empty()) continue;

				// A name that is a Duckling keyword cannot be renamed, because it is the linked
				// identifier, so the record has to go.
				if (context.registry.claim(record.name).accepted)
					context.emitted_records.insert(record.name);
			}
		}

		/** Writes the classes and the skip comments for everything discoverRecords found. */
		void emitRecords(Context& context) {
			for (const auto& record: context.records) {
				const std::string origin = originOf(record.cursor);

				if (!context.emitted_records.contains(record.name)) {
					// A record that is merely opaque or unnamed is not worth a comment; only an
					// actual rejection is.
					if (!record.rejection.empty()) {
						skip(context, "record", record.name, record.rejection);
						std::get<CSkipped>(context.model.decls.back()).origin = origin;
					}
					continue;
				}

				struct FieldCollector final {
					Context*            context;
					std::vector<CField> fields;
				};

				FieldCollector collector{ .context = &context, .fields = {} };

				clang_visitChildren(
					record.cursor,
					[](CXCursor child, CXCursor, CXClientData data) {
						auto& collected = *static_cast<FieldCollector*>(data);
						if (child.kind != CXCursor_FieldDecl) return CXChildVisit_Continue;

						collected.fields.emplace_back(CField{
							.name = usableFieldName(
								spellingOf(child), collected.fields, collected.fields.size()
							),
							.type = mapType(*collected.context, clang_getCursorType(child)) });

						return CXChildVisit_Continue;
					},
					&collector
				);

				context.model.decls.emplace_back(CRecord{ .emitted_name = record.name,
				                                          .fields = std::move(collector.fields),
				                                          .origin = origin });
			}
		}

		void collectFunction(Context& context, CXCursor cursor) {
			std::string name = spellingOf(cursor);

			if (clang_Cursor_getStorageClass(cursor) == CX_SC_Static) {
				skip(context, "function", std::move(name), "`static` has no external symbol");
				return;
			}

			const CXType type = clang_getCursorType(cursor);
			if (clang_isFunctionTypeVariadic(type) != 0) {
				skip(
					context,
					"function",
					std::move(name),
					"variadic; Duckling needs one `@cffi_variadic_fixed_params` declaration "
					"per concrete call signature"
				);
				return;
			}

			auto verdict = context.registry.claim(name);
			if (!verdict.accepted) {
				skip(context, "function", std::move(name), std::move(verdict.reason));
				return;
			}

			std::vector<CParam> params;
			const int           count = clang_Cursor_getNumArguments(cursor);
			for (int i = 0; i < count; i++) {
				CXCursor argument = clang_Cursor_getArgument(cursor, static_cast<unsigned>(i));
				CType    mapped   = mapType(context, clang_getCursorType(argument));
				if (!isSupported(mapped)) {
					skip(
						context,
						"function",
						name,
						"parameter " + std::to_string(i)
							+ " is not C ABI compatible: " + unsupportedReason(mapped)
					);
					return;
				}

				std::string argument_name = spellingOf(argument);
				if (argument_name.empty() || isDucklingKeyword(argument_name))
					argument_name = "arg" + std::to_string(i);

				params.emplace_back(CParam{ .name = std::move(argument_name),
				                            .type = std::move(mapped) });
			}

			const CXType                 returned = clang_getResultType(type);
			std::shared_ptr<const CType> return_type;
			if (clang_getCanonicalType(returned).kind != CXType_Void) {
				CType mapped = mapType(context, returned);
				if (!isSupported(mapped)) {
					skip(
						context,
						"function",
						name,
						"return type is not C ABI compatible: " + unsupportedReason(mapped)
					);
					return;
				}
				return_type = std::make_shared<const CType>(std::move(mapped));
			}

			context.model.decls.emplace_back(CFunction{ .name        = std::move(name),
			                                            .params      = std::move(params),
			                                            .return_type = std::move(return_type) });
		}

		void collectEnum(Context& context, CXCursor cursor) {
			const CXType underlying = clang_getEnumDeclIntegerType(cursor);
			CType        mapped     = mapType(context, underlying);
			if (!isSupported(mapped)) {
				skip(context, "enum", spellingOf(cursor), "underlying type is not C ABI compatible");
				return;
			}

			const std::string rendered = renderType(mapped);
			const bool        is_signed
				= clang_Type_getSizeOf(underlying) > 0 && rendered.starts_with('i');

			std::string tag = spellingOf(cursor);
			if (!tag.empty() && clang_Cursor_isAnonymous(cursor) == 0) {
				std::string name = taggedEnumName(tag);
				if (context.registry.claim(name).accepted)
					context.model.decls.emplace_back(CAlias{ .name   = std::move(name),
					                                         .target = rendered });
			}

			struct EnumCollector final {
				Context*    context;
				std::string rendered;
				bool        is_signed;
			};

			EnumCollector collector{ .context   = &context,
				                     .rendered  = rendered,
				                     .is_signed = is_signed };

			clang_visitChildren(
				cursor,
				[](CXCursor child, CXCursor, CXClientData data) {
					auto& collected = *static_cast<EnumCollector*>(data);
					if (child.kind != CXCursor_EnumConstantDecl) return CXChildVisit_Continue;

					std::string name    = spellingOf(child);
					auto        verdict = collected.context->registry.claim(name);
					if (!verdict.accepted) {
						skip(
							*collected.context,
							"enumerator",
							std::move(name),
							std::move(verdict.reason)
						);
						return CXChildVisit_Continue;
					}

					std::string literal
						= collected.is_signed
				            ? std::to_string(clang_getEnumConstantDeclValue(child))
				            : std::to_string(clang_getEnumConstantDeclUnsignedValue(child));

					collected.context->model.decls.emplace_back(CConst{
						.name    = std::move(name),
						.type    = collected.rendered,
						.literal = literal + collected.rendered });
					return CXChildVisit_Continue;
				},
				&collector
			);
		}

		void collectTypedef(Context& context, CXCursor cursor) {
			std::string name   = spellingOf(cursor);
			CType       mapped = mapType(context, clang_getTypedefDeclUnderlyingType(cursor));

			if (!isSupported(mapped)) {
				skip(context, "typedef", std::move(name), unsupportedReason(mapped));
				return;
			}

			const std::string target = renderType(mapped);
			// `typedef struct X X;` after tag prefixing is not self-referential, but a typedef
			// that resolves to its own name would be.
			if (target == name) return;

			auto verdict = context.registry.claim(name);
			if (!verdict.accepted) {
				skip(context, "typedef", std::move(name), std::move(verdict.reason));
				return;
			}

			context.model.decls.emplace_back(CAlias{ .name = std::move(name), .target = target });
		}

		/** Whether the cursor comes from a real file rather than clang's predefined buffer. */
		bool isFromRealFile(CXCursor cursor) {
			CXFile       file   = nullptr;
			unsigned int line   = 0;
			unsigned int column = 0;
			unsigned int offset = 0;
			clang_getFileLocation(clang_getCursorLocation(cursor), &file, &line, &column, &offset);
			return file != nullptr;
		}

		struct NumericLiteral final {
			std::string type;
			std::string literal;
		};

		/**
		 * @brief Splits a C numeric literal into its digits and its suffix.
		 *
		 * The suffix has to be recognised in full: `F16` and `bf16` carry a width that would
		 * otherwise be left behind in the digits and silently change the value.
		 */
		bool splitLiteral(
			const std::string& text, bool is_float, std::string& digits, bool& is_unsigned
		) {
			std::size_t suffix_start = text.size();
			while (suffix_start > 0) {
				const char character      = text[suffix_start - 1];
				const bool is_suffix_char = character == 'u' || character == 'U' || character == 'l'
				                         || character == 'L' || character == 'f'
				                         || character == 'F';
				if (!is_suffix_char) break;
				suffix_start--;
			}

			digits                   = text.substr(0, suffix_start);
			const std::string suffix = text.substr(suffix_start);
			is_unsigned              = false;

			for (const char character: suffix) {
				if (character == 'u' || character == 'U') {
					if (is_float) return false;
					is_unsigned = true;
				} else if (character == 'f' || character == 'F') {
					if (!is_float) return false;
				}
			}

			return !digits.empty();
		}

		bool parseIntegerLiteral(const std::string& digits, bool is_unsigned, NumericLiteral& out) {
			// Base 0 makes the C prefix authoritative: `0x10` is 16 and `0700` is 448, not 700.
			// Duckling has neither spelling, so the value is re-rendered in decimal.
			try {
				if (is_unsigned) {
					const unsigned long long value = std::stoull(digits, nullptr, 0);
					out = { .type = "u64", .literal = std::to_string(value) + "u64" };
				} else {
					const long long value = std::stoll(digits, nullptr, 0);
					out = { .type = "i64", .literal = std::to_string(value) + "i64" };
				}
			} catch (const std::exception&) { return false; }

			return true;
		}

		/** Accepts an optionally signed integer or floating literal and nothing else. */
		bool renderMacroLiteral(const std::vector<std::string>& body, NumericLiteral& out) {
			if (body.empty() || body.size() > 2) return false;

			std::string        sign;
			const std::string& text = body.back();
			if (body.size() == 2) {
				if (body.front() != "-" && body.front() != "+") return false;
				sign = body.front() == "-" ? "-" : "";
			}

			if (text.empty() || (std::isdigit(static_cast<unsigned char>(text.front())) == 0))
				return false;

			const bool is_hex = text.starts_with("0x") || text.starts_with("0X");
			const bool is_float
				= !is_hex
			   && (text.find('.') != std::string::npos || text.find('e') != std::string::npos
			       || text.find('E') != std::string::npos);

			std::string digits;
			bool        is_unsigned = false;
			if (!splitLiteral(text, is_float, digits, is_unsigned)) return false;

			// Every remaining character has to belong to the number itself.
			for (std::size_t i = 0; i < digits.size(); i++) {
				const char character = digits[i];
				if (std::isdigit(static_cast<unsigned char>(character)) != 0) continue;
				if (is_hex && std::isxdigit(static_cast<unsigned char>(character)) != 0) continue;
				if (is_hex && i == 1 && (character == 'x' || character == 'X')) continue;
				if (is_float
				    && (character == '.' || character == 'e' || character == 'E' || character == '+'
				        || character == '-'))
					continue;
				return false;
			}

			if (is_float) {
				out = { .type = "f64", .literal = sign + digits + "f64" };
				return true;
			}

			// A negative literal cannot be unsigned.
			if (is_unsigned && !sign.empty()) return false;

			if (!parseIntegerLiteral(digits, is_unsigned, out)) return false;
			out.literal = sign + out.literal;
			return true;
		}

		void collectMacro(Context& context, CXTranslationUnit unit, CXCursor cursor) {
			if (clang_Cursor_isMacroFunctionLike(cursor) != 0) return;
			if (clang_Cursor_isMacroBuiltin(cursor) != 0) return;
			// Clang predefines several hundred macros that belong to no header, so they are
			// dropped without a skip comment.
			if (!isFromRealFile(cursor)) return;

			std::string name = spellingOf(cursor);

			CXToken*     tokens = nullptr;
			unsigned int count  = 0;
			clang_tokenize(unit, clang_getCursorExtent(cursor), &tokens, &count);

			std::vector<std::string> body;
			for (unsigned int i = 1; i < count; i++)
				body.emplace_back(OwnedString(clang_getTokenSpelling(unit, tokens[i])).str());

			clang_disposeTokens(unit, tokens, count);

			NumericLiteral rendered;
			if (!renderMacroLiteral(body, rendered)) {
				skip(context, "macro", std::move(name), "value is not a single numeric literal");
				return;
			}

			auto verdict = context.registry.claim(name);
			if (!verdict.accepted) {
				skip(context, "macro", std::move(name), std::move(verdict.reason));
				return;
			}

			context.model.decls.emplace_back(CConst{ .name    = std::move(name),
			                                         .type    = std::move(rendered.type),
			                                         .literal = std::move(rendered.literal) });
		}

	}

	ReadResult readTranslationUnit(const ReadRequest& request) {
		if (request.headers.empty())
			return { .model = {}, .diagnostics = {}, .error = "no headers given" };

		std::ostringstream root;
		for (const auto& header: request.headers) root << "#include \"" << header << "\"\n";
		const std::string root_source = root.str();

		std::vector<std::string> argument_storage = { "-x", "c", request.std_flag };

		// Baked in at configure time: libclang works out a relative `lib/clang/<major>` from its
		// own location, which does not resolve when it is loaded as a library, so every header
		// including <stddef.h>, <stdarg.h> or <float.h> would fail. User arguments come after,
		// so an explicit -resource-dir still wins.
#ifdef DUCK_CLANG_RESOURCE_DIR
		if (std::string_view(DUCK_CLANG_RESOURCE_DIR).empty() == false) {
			argument_storage.emplace_back("-resource-dir");
			argument_storage.emplace_back(DUCK_CLANG_RESOURCE_DIR);
		}
#endif

		argument_storage.insert(
			argument_storage.end(), request.clang_args.begin(), request.clang_args.end()
		);

		std::vector<const char*> arguments;
		arguments.reserve(argument_storage.size());
		for (const auto& argument: argument_storage) arguments.push_back(argument.c_str());

		CXUnsavedFile unsaved{ .Filename = "duck_c_import_root.c",
			                   .Contents = root_source.c_str(),
			                   .Length   = static_cast<unsigned long>(root_source.size()) };

		const OwnedIndex     index;
		OwnedTranslationUnit unit(clang_parseTranslationUnit(
			index.get(),
			"duck_c_import_root.c",
			arguments.data(),
			static_cast<int>(arguments.size()),
			&unsaved,
			1,
			CXTranslationUnit_DetailedPreprocessingRecord | CXTranslationUnit_SkipFunctionBodies
		));

		if (!unit.valid())
			return { .model = {}, .diagnostics = {}, .error = "clang could not parse the headers" };

		ReadResult result;
		bool       fatal = false;
		for (unsigned int i = 0; i < clang_getNumDiagnostics(unit.get()); i++) {
			CXDiagnostic diagnostic = clang_getDiagnostic(unit.get(), i);
			const auto   severity   = clang_getDiagnosticSeverity(diagnostic);
			result.diagnostics.emplace_back(
				OwnedString(
					clang_formatDiagnostic(diagnostic, clang_defaultDiagnosticDisplayOptions())
				)
					.str()
			);
			if (severity >= CXDiagnostic_Error) fatal = true;
			clang_disposeDiagnostic(diagnostic);
		}

		if (fatal && !request.ignore_parse_errors) {
			result.error = "clang reported errors while parsing the headers";
			return result;
		}

		Context context;

		// An anonymous record behind a typedef takes the typedef's name, so those pairings are
		// registered before any record is named.
		clang_visitChildren(
			clang_getTranslationUnitCursor(unit.get()),
			[](CXCursor cursor, CXCursor, CXClientData data) {
				auto& ctx = *static_cast<Context*>(data);
				if (cursor.kind != CXCursor_TypedefDecl) return CXChildVisit_Continue;

				const CXType underlying
					= clang_getCanonicalType(clang_getTypedefDeclUnderlyingType(cursor));
				if (underlying.kind != CXType_Record) return CXChildVisit_Continue;

				CXCursor declaration = clang_getTypeDeclaration(underlying);
				if (clang_Cursor_isAnonymous(declaration) == 0) return CXChildVisit_Continue;

				ctx.anonymous_names.emplace(usrOf(declaration), spellingOf(cursor));
				return CXChildVisit_Continue;
			},
			&context
		);

		discoverRecords(context, clang_getTranslationUnitCursor(unit.get()));
		emitRecords(context);

		struct Visit final {
			Context*          context;
			CXTranslationUnit unit;
		};

		Visit visit{ .context = &context, .unit = unit.get() };

		clang_visitChildren(
			clang_getTranslationUnitCursor(unit.get()),
			[](CXCursor cursor, CXCursor, CXClientData data) {
				auto& [ctx, translation_unit] = *static_cast<Visit*>(data);

				// Each declaration added below is tagged with the header it came from, which is
			    // what `--split` groups by.
				const std::size_t before = ctx->model.decls.size();

				switch (cursor.kind) {
				case CXCursor_EnumDecl:
					if (clang_isCursorDefinition(cursor) != 0) collectEnum(*ctx, cursor);
					break;
				case CXCursor_TypedefDecl:
					collectTypedef(*ctx, cursor);
					break;
				case CXCursor_FunctionDecl:
					collectFunction(*ctx, cursor);
					break;
				case CXCursor_MacroDefinition:
					collectMacro(*ctx, translation_unit, cursor);
					break;
				default:
					break;
				}

				const std::string origin = originOf(cursor);
				for (std::size_t i = before; i < ctx->model.decls.size(); i++)
					std::visit(
						[&origin](auto& declaration) { declaration.origin = origin; },
						ctx->model.decls[i]
					);

				return CXChildVisit_Continue;
			},
			&visit
		);

		result.model = std::move(context.model);
		return result;
	}

}
