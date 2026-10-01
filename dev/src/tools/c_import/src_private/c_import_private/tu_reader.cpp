#include <c_import/tu_reader.hpp>

#include <clang-c/Index.h>

#include <algorithm>
#include <filesystem>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <set>

namespace c_import {

	namespace {

		constexpr std::string_view UNSAVED_FILE = "__duck_c_import.c";
		constexpr std::string_view MACRO_PREFIX = "__duck_m_";
		constexpr std::string_view CHAR_PROBE   = "__duck_char_probe";

		// ============================== RAII ==============================

		std::string take(CXString text) {
			const char* c_str  = clang_getCString(text);
			std::string result = c_str == nullptr ? std::string{} : std::string{ c_str };
			clang_disposeString(text);
			return result;
		}

		struct IndexDeleter final {
			void operator()(void* index) const { clang_disposeIndex(index); }
		};

		struct TranslationUnitDeleter final {
			void operator()(CXTranslationUnit tu) const { clang_disposeTranslationUnit(tu); }
		};

		struct EvalDeleter final {
			void operator()(void* result) const { clang_EvalResult_dispose(result); }
		};

		using Index           = std::unique_ptr<void, IndexDeleter>;
		using TranslationUnit = std::unique_ptr<CXTranslationUnitImpl, TranslationUnitDeleter>;
		using EvalResult      = std::unique_ptr<void, EvalDeleter>;

		/// Visits the direct children of `parent`.
		void forEachChild(CXCursor parent, std::function<void(CXCursor)> visit) {
			clang_visitChildren(
				parent,
				[](CXCursor child, CXCursor, CXClientData data) {
					(*static_cast<std::function<void(CXCursor)>*>(data))(child);
					return CXChildVisit_Continue;
				},
				&visit
			);
		}

		std::string fileName(CXFile file) {
			if (file == nullptr) return {};
			auto real = take(clang_File_tryGetRealPathName(file));
			if (!real.empty()) return real;
			return take(clang_getFileName(file));
		}

		// ============================== source ==============================

		/// The translation unit including every requested header.
		std::string includeSource(const std::vector<std::string>& headers) {
			std::string source;
			for (const auto& header: headers) {
				std::error_code ec;
				if (std::filesystem::is_regular_file(header, ec))
					source += std::format(
						"#include \"{}\"\n", std::filesystem::absolute(header).string()
					);
				else
					source += std::format("#include <{}>\n", header);
			}
			source += std::format("typedef char {};\n", CHAR_PROBE);
			return source;
		}

		TranslationUnit parse(
			CXIndex index, const std::string& source, const ReadOptions& options, unsigned extra_flags
		) {
			std::vector<std::string> args{ "-x", "c" };
			if (!options.resource_dir.empty()) {
				args.emplace_back("-resource-dir");
				args.push_back(options.resource_dir);
			}
			std::ranges::copy(options.clang_args, std::back_inserter(args));
			std::vector<const char*> argv;
			argv.reserve(args.size());
			for (const auto& arg: args) argv.push_back(arg.c_str());

			const std::string unsaved_name{ UNSAVED_FILE };
			CXUnsavedFile     unsaved{ .Filename = unsaved_name.c_str(),
				                       .Contents = source.c_str(),
				                       .Length   = source.size() };

			CXTranslationUnit tu = nullptr;
			const unsigned    flags
				= CXTranslationUnit_SkipFunctionBodies | CXTranslationUnit_KeepGoing | extra_flags;
			clang_parseTranslationUnit2(
				index,
				unsaved_name.c_str(),
				argv.data(),
				static_cast<int>(argv.size()),
				&unsaved,
				1,
				flags,
				&tu
			);
			return TranslationUnit{ tu };
		}

		std::vector<std::string> errorsOf(CXTranslationUnit tu) {
			std::vector<std::string> errors;
			const unsigned           count = clang_getNumDiagnostics(tu);
			for (unsigned i = 0; i < count; ++i) {
				CXDiagnostic diagnostic = clang_getDiagnostic(tu, i);
				if (clang_getDiagnosticSeverity(diagnostic) >= CXDiagnostic_Error)
					errors.push_back(take(
						clang_formatDiagnostic(diagnostic, clang_defaultDiagnosticDisplayOptions())
					));
				clang_disposeDiagnostic(diagnostic);
			}
			return errors;
		}

		// ============================== reader ==============================

		class Reader final {
		public:
			Reader(CXTranslationUnit tu, const std::vector<std::string>& headers): tu(tu) {
				collectRequestedScope(headers);
			}

			void read() {
				forEachChild(clang_getTranslationUnitCursor(tu), [&](CXCursor cursor) {
					visitTopLevel(cursor);
				});
				// A record declared by `struct { ... } field;` is named after its field, once the
				// parent's own name (which may come from a later typedef) is known.
				for (const auto& [inner, parent, field]: named_after_field) {
					auto& record = model.records[inner];
					if (typedef_named.contains(inner) || !record.name.empty()) continue;
					record.name = std::format("{}_{}", model.records[parent].name, field);
				}
			}

			CModel                   model;
			std::vector<std::string> macros;

		private:
			struct NamedAfterField final {
				std::size_t inner;
				std::size_t parent;
				std::string field;
			};

			CXTranslationUnit                  tu;
			std::vector<std::string>           requested_directories;
			std::set<std::string>              requested_files;
			std::set<std::size_t>              typedef_named;
			std::set<std::size_t>              filling;
			std::vector<NamedAfterField>       named_after_field;
			std::map<std::string, std::size_t> record_by_usr;
			std::map<std::string, std::size_t> enum_by_usr;
			std::set<std::string>              seen_functions;
			std::set<std::string>              ordinary_names;

			// ---------- locations ----------

			/**
			 * @brief A header given by path claims its whole directory, and one named with a
			 * directory (`SDL3/SDL.h`) claims that directory. A bare name (`math.h`) claims its own
			 * file and the files it includes, directly or not, from below its directory: its
			 * directory is usually the system include directory, and glibc declares much of a
			 * header's API in `bits/`.
			 */
			void collectRequestedScope(const std::vector<std::string>& headers) {
				struct Inclusion final {
					std::string              file;
					std::vector<std::string> includers;
				};

				std::vector<Inclusion> inclusions;
				clang_getInclusions(
					tu,
					[](CXFile file, CXSourceLocation* stack, unsigned depth, CXClientData data) {
						Inclusion inclusion{ .file = fileName(file), .includers = {} };
						for (unsigned i = 0; i < depth; ++i) {
							CXFile includer = nullptr;
							clang_getExpansionLocation(
								stack[i], &includer, nullptr, nullptr, nullptr
							);
							inclusion.includers.push_back(fileName(includer));
						}
						static_cast<std::vector<Inclusion>*>(data)->push_back(std::move(inclusion));
					},
					&inclusions
				);
				auto is_direct
					= [](const Inclusion& inclusion) { return inclusion.includers.size() == 1; };

				for (const auto& header: headers) {
					std::error_code ec;
					if (std::filesystem::is_regular_file(header, ec)) {
						auto path = std::filesystem::weakly_canonical(header, ec);
						requested_directories.push_back(path.parent_path().string());
						continue;
					}
					auto match = std::ranges::find_if(inclusions, [&](const Inclusion& inclusion) {
						return is_direct(inclusion) && inclusion.file.ends_with("/" + header);
					});
					if (match == inclusions.end()) continue;
					const std::string& requested = match->file;
					if (header.contains('/')) {
						requested_directories.push_back(
							requested.substr(0, requested.size() - header.size() + header.find('/'))
						);
						continue;
					}
					requested_files.insert(requested);
					auto directory = std::filesystem::path(requested).parent_path().string() + "/";
					for (const auto& inclusion: inclusions)
						if (inclusion.file.starts_with(directory)
						    && std::ranges::contains(inclusion.includers, requested))
							requested_files.insert(inclusion.file);
				}
			}

			[[nodiscard]] CLocation locationOf(CXCursor cursor) const {
				CXFile file = nullptr;
				clang_getExpansionLocation(
					clang_getCursorLocation(cursor), &file, nullptr, nullptr, nullptr
				);
				CLocation location{ .file = fileName(file), .in_requested_headers = false };
				location.in_requested_headers = requested_files.contains(location.file);
				for (const auto& directory: requested_directories) {
					if (location.file.starts_with(directory + "/")) {
						location.in_requested_headers = true;
						break;
					}
				}
				return location;
			}

			// ---------- top level ----------

			void visitTopLevel(CXCursor cursor) {
				switch (clang_getCursorKind(cursor)) {
				case CXCursor_StructDecl:
				case CXCursor_UnionDecl:
					recordIndex(cursor);
					break;
				case CXCursor_TypedefDecl:
					visitTypedef(cursor);
					break;
				case CXCursor_EnumDecl:
					visitEnum(cursor);
					break;
				case CXCursor_FunctionDecl:
					visitFunction(cursor);
					break;
				case CXCursor_VarDecl:
					visitVariable(cursor);
					break;
				case CXCursor_MacroDefinition:
					visitMacro(cursor);
					break;
				default:
					break;
				}
			}

			void visitTypedef(CXCursor cursor) {
				auto name = take(clang_getCursorSpelling(cursor));
				if (name == CHAR_PROBE) {
					auto kind
						= clang_getCanonicalType(clang_getTypedefDeclUnderlyingType(cursor)).kind;
					model.char_is_signed = kind == CXType_Char_S;
					return;
				}
				CXType underlying
					= clang_getCanonicalType(clang_getTypedefDeclUnderlyingType(cursor));
				if (underlying.kind == CXType_Record) {
					auto  index  = recordIndex(clang_getTypeDeclaration(underlying));
					auto& record = model.records[index];
					// The typedef is what C code spells, so it names the record; the first one wins.
					if (typedef_named.insert(index).second) {
						record.name      = name;
						record.anonymous = false;
					}
				} else if (underlying.kind == CXType_Enum) {
					enumIndex(clang_getTypeDeclaration(underlying));
				}
			}

			// ---------- records ----------

			std::size_t recordIndex(CXCursor cursor) {
				CXCursor canonical = clang_getCanonicalCursor(cursor);
				auto     usr       = take(clang_getCursorUSR(canonical));
				if (auto it = record_by_usr.find(usr); it != record_by_usr.end()) {
					fillRecord(it->second, canonical);
					return it->second;
				}
				auto index = model.records.size();
				record_by_usr.emplace(usr, index);

				CRecord record;
				record.is_union  = clang_getCursorKind(canonical) == CXCursor_UnionDecl;
				record.anonymous = clang_Cursor_isAnonymous(canonical) != 0;
				if (!record.anonymous) record.name = take(clang_getCursorSpelling(canonical));
				record.location = locationOf(canonical);
				model.records.push_back(std::move(record));

				fillRecord(index, canonical);
				return index;
			}

			void fillRecord(std::size_t index, CXCursor canonical) {
				if (model.records[index].complete || filling.contains(index)) return;
				CXCursor definition = clang_getCursorDefinition(canonical);
				if (clang_Cursor_isNull(definition)) return;

				filling.insert(index);
				CXType              type = clang_getCursorType(definition);
				std::vector<CField> fields;
				forEachChild(definition, [&](CXCursor child) {
					auto kind = clang_getCursorKind(child);
					if (kind == CXCursor_FieldDecl) {
						fields.push_back(readField(child, index));
					} else if ((kind == CXCursor_StructDecl || kind == CXCursor_UnionDecl)
					           && clang_Cursor_isAnonymousRecordDecl(child)) {
						if (auto field = anonymousMember(child, type); field.has_value())
							fields.push_back(std::move(*field));
					}
				});

				auto& record  = model.records[index];
				record.fields = std::move(fields);
				record.size
					= static_cast<std::uint64_t>(std::max<long long>(clang_Type_getSizeOf(type), 0));
				record.align
					= static_cast<std::uint64_t>(std::max<long long>(clang_Type_getAlignOf(type), 0)
				    );
				record.location = locationOf(definition);
				record.complete = true;
				filling.erase(index);
			}

			CField readField(CXCursor cursor, std::size_t parent) {
				CField field;
				field.name        = take(clang_getCursorSpelling(cursor));
				CXType type       = clang_getCursorType(cursor);
				field.type        = mapType(type);
				field.offset_bits = static_cast<std::uint64_t>(
					std::max<long long>(clang_Cursor_getOffsetOfField(cursor), 0)
				);
				if (clang_Cursor_isBitField(cursor))
					field.bit_width
						= static_cast<std::uint32_t>(clang_getFieldDeclBitWidth(cursor));
				field.size
					= static_cast<std::uint64_t>(std::max<long long>(clang_Type_getSizeOf(type), 0));
				field.align
					= static_cast<std::uint64_t>(std::max<long long>(clang_Type_getAlignOf(type), 0)
				    );

				// `struct { ... } field;` declares a record with no tag, named after its field here.
				if (const auto* ref = std::get_if<CRecordRef>(&field.type->kind)) {
					auto& inner = model.records[ref->index];
					if (inner.anonymous && !field.name.empty()) {
						inner.anonymous = false;
						named_after_field.push_back({ ref->index, parent, field.name });
					}
				}
				return field;
			}

			/// An unnamed struct or union member; its offset is found through a named field inside it.
			std::optional<CField> anonymousMember(CXCursor cursor, CXType parent_type) {
				auto        index = recordIndex(cursor);
				const auto& inner = model.records[index];
				auto        named = std::ranges::find_if(inner.fields, [](const CField& f) {
                    return !f.name.empty();
                });
				if (named == inner.fields.end()) return std::nullopt;
				long long outer_offset = clang_Type_getOffsetOf(parent_type, named->name.c_str());
				if (outer_offset < 0) return std::nullopt;

				CField field;
				field.type        = makeType(CRecordRef{ index });
				field.offset_bits = static_cast<std::uint64_t>(outer_offset) - named->offset_bits;
				field.size        = inner.size;
				field.align       = inner.align;

				return field;
			}

			// ---------- types ----------

			CTypeRef mapType(CXType type) {
				CXType canonical = clang_getCanonicalType(type);
				auto   bits      = [&] {
                    return static_cast<std::uint32_t>(clang_Type_getSizeOf(canonical) * 8);
				};
				switch (canonical.kind) {
				case CXType_Void:
					return makeType(CVoid{});
				case CXType_Bool:
					return makeType(CScalar{ .kind = ScalarKind::Bool, .bits = 8 });
				case CXType_Char_S:
				case CXType_Char_U:
					return makeType(CScalar{ .kind = ScalarKind::Char, .bits = 8 });
				case CXType_SChar:
				case CXType_Short:
				case CXType_Int:
				case CXType_Long:
				case CXType_LongLong:
				case CXType_Int128:
					return makeType(CScalar{ .kind = ScalarKind::SignedInt, .bits = bits() });
				case CXType_UChar:
				case CXType_UShort:
				case CXType_UInt:
				case CXType_ULong:
				case CXType_ULongLong:
				case CXType_UInt128:
				case CXType_Char16:
				case CXType_Char32:
					return makeType(CScalar{ .kind = ScalarKind::UnsignedInt, .bits = bits() });
				case CXType_Float:
				case CXType_Double:
					return makeType(CScalar{ .kind = ScalarKind::Float, .bits = bits() });
				case CXType_LongDouble:
					return makeType(CUnsupported{ "`long double` has no mapping (#1498)" });
				case CXType_Pointer:
					return makeType(CPointer{ mapType(clang_getPointeeType(canonical)) });
				case CXType_ConstantArray:
					return makeType(CArray{
						.element = mapType(clang_getArrayElementType(canonical)),
						.count   = static_cast<std::uint64_t>(clang_getArraySize(canonical)) });
				case CXType_IncompleteArray:
					return makeType(CArray{
						.element = mapType(clang_getArrayElementType(canonical)), .count = 0 });
				case CXType_Record:
					return makeType(CRecordRef{ recordIndex(clang_getTypeDeclaration(canonical)) });
				case CXType_Enum: {
					CXCursor declaration = clang_getTypeDeclaration(canonical);
					enumIndex(declaration);
					return mapType(clang_getEnumDeclIntegerType(declaration));
				}
				case CXType_FunctionProto:
				case CXType_FunctionNoProto:
					return makeType(CFunctionType{});
				default:
					return makeType(CUnsupported{
						std::format("`{}` has no mapping", take(clang_getTypeSpelling(canonical))) }
					);
				}
			}

			// ---------- enums ----------

			void enumIndex(CXCursor cursor) {
				CXCursor definition = clang_getCursorDefinition(clang_getCanonicalCursor(cursor));
				if (clang_Cursor_isNull(definition)) return;
				auto usr = take(clang_getCursorUSR(definition));
				if (enum_by_usr.contains(usr)) return;
				enum_by_usr.emplace(usr, model.enums.size());

				CEnum enumeration;
				if (!clang_Cursor_isAnonymous(definition))
					enumeration.name = take(clang_getCursorSpelling(definition));
				auto underlying        = mapType(clang_getEnumDeclIntegerType(definition));
				enumeration.underlying = std::get<CScalar>(underlying->kind);
				enumeration.location   = locationOf(definition);
				const bool is_unsigned
					= enumeration.underlying.kind == ScalarKind::UnsignedInt
				   || (enumeration.underlying.kind == ScalarKind::Char && !model.char_is_signed);
				forEachChild(definition, [&](CXCursor child) {
					if (clang_getCursorKind(child) != CXCursor_EnumConstantDecl) return;
					CEnumerator enumerator;
					enumerator.name = take(clang_getCursorSpelling(child));
					if (is_unsigned)
						enumerator.value = clang_getEnumConstantDeclUnsignedValue(child);
					else
						enumerator.value
							= static_cast<std::int64_t>(clang_getEnumConstantDeclValue(child));
					ordinary_names.insert(enumerator.name);
					enumeration.enumerators.push_back(std::move(enumerator));
				});
				model.enums.push_back(std::move(enumeration));
			}

			void visitEnum(CXCursor cursor) { enumIndex(cursor); }

			// ---------- functions and variables ----------

			void visitFunction(CXCursor cursor) {
				auto name = take(clang_getCursorSpelling(cursor));
				ordinary_names.insert(name);
				if (!seen_functions.insert(name).second) return;

				CFunction function;
				function.name             = name;
				CXType type               = clang_getCursorType(cursor);
				function.return_type      = mapType(clang_getResultType(type));
				function.variadic         = clang_isFunctionTypeVariadic(type) != 0;
				function.internal_linkage = clang_Cursor_getStorageClass(cursor) == CX_SC_Static;
				function.location         = locationOf(cursor);
				const int count           = clang_Cursor_getNumArguments(cursor);
				for (int k = 0; k < count; ++k) {
					CXCursor argument = clang_Cursor_getArgument(cursor, static_cast<unsigned>(k));
					function.params.push_back({ take(clang_getCursorSpelling(argument)),
					                            mapType(clang_getCursorType(argument)) });
				}
				model.functions.push_back(std::move(function));
			}

			void visitVariable(CXCursor cursor) {
				auto name = take(clang_getCursorSpelling(cursor));
				if (name.starts_with(MACRO_PREFIX)) return;
				ordinary_names.insert(name);
				auto location = locationOf(cursor);

				if (clang_Cursor_getStorageClass(cursor) == CX_SC_Static) {
					if (auto constant = evaluate(cursor, name, location); constant.has_value()) {
						model.constants.push_back(std::move(*constant));
						return;
					}
				}
				model.unreadable.push_back(
					{ name, "global variables are not supported", std::move(location) }
				);
			}

			// ---------- macros ----------

			void visitMacro(CXCursor cursor) {
				if (clang_Cursor_isMacroBuiltin(cursor) || clang_Cursor_isMacroFunctionLike(cursor))
					return;
				if (!locationOf(cursor).in_requested_headers) return;
				auto name = take(clang_getCursorSpelling(cursor));
				if (!std::ranges::contains(macros, name)) macros.push_back(std::move(name));
			}

		public:
			/// The macros that do not name something the headers already declare.
			[[nodiscard]] std::vector<std::string> macrosToEvaluate() const {
				std::vector<std::string> result;
				for (const auto& name: macros)
					if (!ordinary_names.contains(name)) result.push_back(name);
				return result;
			}

			std::optional<CConstant> evaluate(CXCursor cursor, std::string name, CLocation location) {
				if (clang_isInvalidDeclaration(cursor)) return std::nullopt;
				auto        type   = mapType(clang_getCursorType(cursor));
				const auto* scalar = std::get_if<CScalar>(&type->kind);
				if (scalar == nullptr) return std::nullopt;

				EvalResult result{ clang_Cursor_Evaluate(cursor) };
				if (!result) return std::nullopt;
				CConstant constant{ .name     = std::move(name),
					                .type     = *scalar,
					                .value    = std::int64_t{ 0 },
					                .location = std::move(location) };
				switch (clang_EvalResult_getKind(result.get())) {
				case CXEval_Int:
					if (clang_EvalResult_isUnsignedInt(result.get())
					    || scalar->kind == ScalarKind::UnsignedInt
					    || scalar->kind == ScalarKind::Bool)
						constant.value
							= static_cast<std::uint64_t>(clang_EvalResult_getAsUnsigned(result.get()
						    ));
					else
						constant.value
							= static_cast<std::int64_t>(clang_EvalResult_getAsLongLong(result.get())
						    );
					break;
				case CXEval_Float:
					constant.value = clang_EvalResult_getAsDouble(result.get());
					break;
				default:
					return std::nullopt;
				}
				return constant;
			}
		};

		/// Evaluates each macro as the initializer of a `static const __auto_type`, so clang
		/// decides both its value and its type.
		void evaluateMacros(
			CXIndex            index,
			const ReadOptions& options,
			Reader&            reader,
			const std::string& source,
			ReadResult&        result
		) {
			auto macros = reader.macrosToEvaluate();
			if (macros.empty()) return;

			std::string evaluation = source;
			for (const auto& name: macros)
				evaluation += std::format(
					"static const __auto_type {}{} = ({});\n", MACRO_PREFIX, name, name
				);
			auto tu = parse(index, evaluation, options, 0);
			if (!tu) {
				result.non_numeric_macros += macros.size();
				return;
			}

			std::set<std::string> evaluated;
			forEachChild(clang_getTranslationUnitCursor(tu.get()), [&](CXCursor cursor) {
				if (clang_getCursorKind(cursor) != CXCursor_VarDecl) return;
				auto spelling = take(clang_getCursorSpelling(cursor));
				if (!spelling.starts_with(MACRO_PREFIX)) return;
				auto name = spelling.substr(MACRO_PREFIX.size());
				// The location is the macro's, since the variable itself lives in the synthesized file.
				auto macro_location = CLocation{ .file = {}, .in_requested_headers = true };
				if (auto constant = reader.evaluate(cursor, name, macro_location);
				    constant.has_value()) {
					evaluated.insert(name);
					reader.model.constants.push_back(std::move(*constant));
				}
			});
			result.non_numeric_macros += macros.size() - evaluated.size();
		}

	}

	ReadResult readHeaders(const ReadOptions& options) {
		ReadResult result;
		Index      index{ clang_createIndex(0, 0) };
		auto       source = includeSource(options.headers);
		auto       tu
			= parse(index.get(), source, options, CXTranslationUnit_DetailedPreprocessingRecord);
		if (!tu) {
			result.errors.emplace_back("libclang could not parse the headers");
			return result;
		}
		result.errors = errorsOf(tu.get());

		Reader reader{ tu.get(), options.headers };
		reader.read();
		evaluateMacros(index.get(), options, reader, source, result);
		result.model = std::move(reader.model);
		return result;
	}

}
