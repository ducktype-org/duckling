#include "mangler.hpp"

#include <concurrent/base/collections/hash_map.hpp>
#include <ctv/ctv.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/element_kind.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <global_state/packages.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/scope_id.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/symbol_abi.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/pst_layer/pst_parent.hpp>
#include <helios_private/scopes/scope_data.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <helios_private/templates/templates.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <hashing/hash.hpp>
#include <logger/logger.hpp>
#include <query_framework/standard_query/query_impl.hpp>
#include <unicode_classification/classifications.hpp>

#include <algorithm>
#include <sstream>
#include <string_view>

/**
 * This is the implementation of the mangling scheme according to mangling-scheme.md
 * If any changes are made to this file, please also update the mangling-scheme.md
 * That file provides a detailed description and motivation for some design choices made here
 */
namespace compiler::helios::mangler {

	using namespace std::literals::string_view_literals;

	void addToHash(hashing::hash_algorithm auto& h, const KeyOf_MangledSymbol& k) RELEASE_NOEXCEPT {
		addToHash(h, k.symbol_key.index());

		variant_match(k.symbol_key) {
			variant_case(SymID, val) { addToHash(h, val); }
			variant_case(special_symbol_keys::LIRModuleID, val) { addToHash(h, val); }
			variant_default {
				CORE_PANIC("KeyOf_MangledSymbol has an unexpected symbol_key index");
			}
		}

		addToHash(h, k.kind);
		addToHash(h, k.mangling_scheme_version);
		addToHash(h, k.additional_metadata.has_value());
		if (k.additional_metadata) addToHash(h, k.additional_metadata.value());
	}

	base::Bit256 KeyOf_MangledSymbol::queryUnstablePerfectHash() const {
		return hashing::justHash<hashing::SHA256>(*this);
	}

	namespace internal {
		static constexpr auto BASE_62_DIGITS
			= "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"sv;

		/**
		 * @brief Check if the symbol should be mangled in the first place.
		 * @note: See mangling-scheme.md for details
		 */
		bool shouldMangle(query::Context& ctx, const KeyOf_MangledSymbol& key) {
			if (key.kind != ManglingSymbolKind::Standard) {
				// Non-standard symbols can't have C mangling
				return true;
			}

			const auto sym_id = std::get<SymID>(key.symbol_key);
			if (auto abi = ctx.query<QuerySymbolABI>(sym_id); abi->hasValue()) {
				variant_match(abi->valueOrThrow()) {
					variant_case_novalue(CAbi) { return false; }
					variant_case_novalue(DVMAbi) { return false; }
					variant_case_novalue(DefaultAbi) { return true; }
					variant_default { CORE_PANIC("Unknown ABI in shouldMangle()"); }
				}
			}

			return true;
		}

		/**
		 * @brief A shorter representation of a number in base-62, used to save space
		 * @note: See mangling-scheme.md for details
		 */
		std::string compactNumber(u64 number) {
			if (number == 0) return "_";

			constexpr u64 BASE = BASE_62_DIGITS.size();

			std::string ret;
			--number;
			do {
				ret += BASE_62_DIGITS.at(number % BASE);
				number /= BASE;
			} while (number > 0);
			std::ranges::reverse(ret);
			ret += '_';
			return ret;
		}

		constexpr base::Optional<const char> findExtendedChar(std::string_view str) {
			auto it = std::ranges::find_if_not(str, [](char c) -> bool {
				return c == '_' || BASE_62_DIGITS.contains(c);
			});

			if (it != str.end())
				return *it;
			else
				return std::nullopt;
		}

		auto quoteExtendedChar(const char c) {
			if (std::isprint(static_cast<unsigned char>(c)))
				return base::strConcat(
					'\'', c, "' (int: ", static_cast<int>(static_cast<unsigned char>(c)), ")"
				);
			else
				return base::strConcat(
					"[not-printable] (int: ", static_cast<int>(static_cast<unsigned char>(c)), ")"
				);
		}

		/**
		 * @brief Returns bare identifier of the symbol prefixed with its size
		 * If the name contains characters outside of the set allowed in symbols,
		 * it will be prefixed with 'U' and punycode-encoded.
		 * @note: See mangling-scheme.md for details
		 */
		void identifier(std::stringstream& ss, std::string_view name) {
			// @todo: #3285 add backreferences
			if (not findExtendedChar(name)) {
				ss << name.size() << name;
				return;
			} else {
				constexpr char UNICODE_PREFIX = 'U';
				// @todo: #3286 convert to punycode
				std::string_view puny_string = name;
				ss << UNICODE_PREFIX << puny_string.size() << puny_string;
			}
		}

		/**
		 * @brief Returns bare identifier of the symbol prefixed with its size
		 * If the name contains characters outside of the set allowed in symbols,
		 * it will be prefixed with 'U' and punycode-encoded.
		 * @note: See mangling-scheme.md for details
		 */
		std::string identifier(std::string_view name) {
			std::stringstream ss;
			identifier(ss, name);
			return ss.str();
		}

		/**
		 * @brief Returns package/module/script prefix for the symbol
		 * @note: See mangling-scheme.md for details
		 */
		std::string pathPrefix(query::Context& ctx, SymID symbol_id) {
			// @future: add scripts after #2762
			std::stringstream ret_ss;

			// package/module prefix
			const auto symbol_module = module(scope(symbol_id));
			const auto package_id
				= compiler::frontend::getModuleRef(symbol_module)->getPackage().unlock(ctx).getID();
			std::cerr << "@taw3e8 halo 1\n";
			if (const auto package_ref_opt = global_state::getPackageRefOpt(package_id)) {
				std::cerr << "@taw3e8 halo 2\n";
				/* we're in a package */
				auto raw_package_name = package_ref_opt.value()->getName().str();
				// @todo: #3286 for now we allow '-' and just treat it as '_'
				std::ranges::replace(raw_package_name, '-', '_');
				auto package_name = identifier(raw_package_name);

				if (const auto c = findExtendedChar(package_name)) {
					std::cerr << "@taw3e8 halo 3\n";
					ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(base::strConcat(
						"Name of a package contains a character that is not allowed yet: ",
						quoteExtendedChar(c.value())
					)));
				}
				ret_ss << 'P' << package_name;
			} else {
				/* standalone module */
				ret_ss << 'M';
			}

			// <module-name>+
			// @future: templated modules
			{
				std::vector<std::string_view> modules;
				modules.push_back(frontend::moduleName(symbol_module).strView());

				for (auto current_module = symbol_module;
				     auto parent_module = ctx.query<frontend::QueryParentModule>(current_module);) {
					current_module = parent_module.value();
					modules.push_back(frontend::moduleName(current_module).strView());
				}

				for (auto mod: modules | std::views::reverse) identifier(ret_ss, mod);
			}

			auto ret = ret_ss.str();
			// @todo: #3286 for now we allow '-' and just treat it as '_'
			std::ranges::replace(ret, '-', '_');
			if (const auto c = findExtendedChar(ret))
				ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(base::strConcat(
					"Name of a module contains a character that is not allowed yet: ",
					quoteExtendedChar(c.value())
				)));

			// @todo: #3285 add backreferences
			return ret;
		}

		std::string pathPrefix(special_symbol_keys::LIRModuleID mod_id) {
			// @todo: #2643 rething compiler-generated symbols
			// @todo: #3285 add backreferences
			return base::strConcat("M", mod_id.id);
		}

		/**
		 * @brief Returns the fixed 2-letter mnemonic tag for a single recognized operator
		 * character, or an empty `Optional` if the codepoint isn't one of them.
		 * @note: See mangling-scheme.md's `<fixed-operator-tag>` for details
		 */
		base::Optional<std::string_view> fixedOperatorTag(UChar32 codepoint) {
			switch (codepoint) {
			case U'!':
				return std::string_view{ "nt" };
			case U'%':
				return std::string_view{ "rm" };
			case U'&':
				return std::string_view{ "an" };
			case U'*':
				return std::string_view{ "ml" };
			case U'+':
				return std::string_view{ "pl" };
			case U'-':
				return std::string_view{ "mi" };
			case U'.':
				return std::string_view{ "pd" };
			case U'/':
				return std::string_view{ "dv" };
			case U':':
				return std::string_view{ "co" };
			case U'<':
				return std::string_view{ "lt" };
			case U'=':
				return std::string_view{ "eq" };
			case U'>':
				return std::string_view{ "gt" };
			case U'?':
				return std::string_view{ "qm" };
			case U'\\':
				return std::string_view{ "bs" };
			case U'^':
				return std::string_view{ "eo" };
			case U'`':
				return std::string_view{ "bt" };
			case U'|':
				return std::string_view{ "or" };
			case U'~':
				return std::string_view{ "ti" };
			default:
				return {};
			}
		}

		/**
		 * @brief Transliterates an operator name character-by-character: each recognized operator
		 * character becomes its fixed 2-letter tag, and any other codepoint is escaped as
		 * `"x" <hex-codepoint> "_"`.
		 * @note: See mangling-scheme.md's `<translit-unit>` for details
		 */
		std::string transliterateOperatorName(std::string_view raw_name) {
			const icu::UnicodeString unicode_name = icu::UnicodeString::fromUTF8(
				icu::StringPiece(raw_name.data(), static_cast<i32>(raw_name.size()))
			);
			const i32 length = unicode_name.length();

			std::stringstream ret;
			i32               index = 0;
			while (index < length) {
				const UChar32                          codepoint = unicode_name.char32At(index);
				const base::Optional<std::string_view> tag       = fixedOperatorTag(codepoint);
				if (tag.has_value())
					ret << tag.value();
				else
					ret << "x" << std::hex << static_cast<u32>(codepoint) << std::dec << "_";
				index = unicode_name.moveIndex32(index, 1);
			}
			return ret.str();
		}

		/**
		 * @brief Returns the mangled `<operator-name>` for an operator function/method.
		 * @note: See mangling-scheme.md's `<operator-name>` for details.
		 */
		std::string operatorNameEncoding(const HOUTFunctionDeclaration& fun_decl) {
			using Operatoriness = HOUTFunctionDeclaration::Operatoriness;

			const char operatoriness_tag = [&] {
				switch (fun_decl.operatoriness) {
				case Operatoriness::Infix:
					return 'i';
				case Operatoriness::Prefix:
					return 'p';
				case Operatoriness::Suffix:
					return 's';
				case Operatoriness::None:
					break;
				}
				CORE_UNREACHABLE();
			}();

			const std::string translit
				= transliterateOperatorName(fun_decl.original_name.strView());

			std::stringstream ret;
			ret << "O" << operatoriness_tag << translit.size() << translit;

			return ret.str();
		}

		/**
		 * @brief Returns the bare name of the symbol prefixed with its size, or the mangled
		 * `<operator-name>` if the symbol is an operator function/method.
		 * @note: See mangling-scheme.md for details
		 */
		std::string unscopedName(query::Context& ctx, SymID symbol_id) {
			// @todo: #3285 add backreferences
			if (isFunctionLike(kind(symbol_id))) {
				const auto& fun_decl
					= ctx.query<compiler::helios::QueryDeclOfFun>(symbol_id).get()->valueOrPanic();
				if (fun_decl.operatoriness != HOUTFunctionDeclaration::Operatoriness::None)
					return operatorNameEncoding(fun_decl);
			}
			return identifier(compiler::helios::name(symbol_id).strView());
		}

		template<typename ElemT>
		auto checkAndAddElem(
			const auto& it, const auto& ancestors, const auto& ancestor, auto& ret, auto& ctx
		) {
			for (auto it_cpy = it; it_cpy != ancestors.rbegin();)
				if ((--it_cpy)->second == ancestor->getID()) return;

			const auto val = ancestor.template dynamicCast<ElemT>().value();

			if constexpr (std::same_as<ElemT, pst::Fun>) {
				auto sym = ctx.template query<QuerySymbolOfSTMT>({ val }).valueOrPanic();
				ret += unscopedName(ctx, sym);

				std::string func(query::Context&, SymID);
				ret += func(ctx, sym);
				return;
			}

			ret += identifier(val->getName().unlock(ctx)->unwrap().strView());
		}

		/**
		 * @brief Returns symbol name prefixed with all enclosing it scopes to uniquely identify it
		 * within a single module.
		 * @note It does not mangle module/package names, pathPrefix and path functions are
		 * responsible for that.
		 * @note See mangling-scheme.md for details
		 */
		std::string symbolName(query::Context& ctx, SymID symbol_id) {
			// fast-path for global symbols that are not templates
			if (scopeDepth(scope(symbol_id)) == 1) return "G" + unscopedName(ctx, symbol_id);

			// This function can be only called for symbols
			// that have clear PST-path mangling.
			// This means that all PstImplementedSemantics work, and few
			// additional cases that are handled below.
			auto ancestor_opt = [&]() {
				variant_match(getSymRef(symbol_id)->other) {
					variant_case_novalue(PstImplementedSemantics, BuiltinSemantics) {
						auto ancestor = maybeSymbolPst(symbol_id).value().unlock(ctx);
						return getPSTElementParent(ctx, ancestor);
					}
					variant_case(defgen::GeneratedConstant, const_data) {
						// @TODO: #2587 adjust code here

						auto scope = const_data.scope;
						auto ancestor
							= ScopeAccess_Functor::get(scope)->relatedPSTElement().value();
						return PSTParentResult{ ancestor };
					}
				}
				CORE_UNREACHABLE();
			}();

			// it's not compiler-generated and it's ancestor is a template statement
			const bool is_template = maybeSymbolPst(symbol_id).has_value()
			                      && ancestor_opt.isLangElement()
			                      && ancestor_opt.getAsLangElement().unlock(ctx)->getElementKind()
			                             == pst::ElementKind::TemplateStmt;

			// for template statements we additionally keep PstID of their inner statement to check
			// if their identifier was already added before
			std::vector<std::pair<pst::Access<pst::LangElement>, base::Optional<pst::PstID>>>
				 ancestors;
			bool is_nested = false;
			while (ancestor_opt.isLangElement()) {
				auto ancestor = ancestor_opt.getAsLangElement().unlock(ctx);
				switch (ancestor->getElementKind()) {
					using enum pst::ElementKind;
				case Namespace:
				case Class:
				case Fun:
					is_nested = true;
					ancestors.emplace_back(ancestor, std::nullopt);
					break;
				case TemplateStmt:
					ancestors.emplace_back(
						ancestor,
						ancestor.dynamicCast<pst::TemplateStmt>()
							.value()
							->getInnerStatement()
							.unlock(ctx)
							->getID()
					);
					break;
				default:
					break;
				}
				ancestor_opt = getPSTElementParent(ctx, ancestor);
			}

			std::string ret = (is_nested ? "N" : "G");
			for (auto it = ancestors.rbegin(); it != ancestors.rend(); ++it) {
				const auto [ancestor, _] = *it;

				// @todo: #3285 add backreferences -- <name-prefix>
				switch (ancestor->getElementKind()) {
					using enum pst::ElementKind;
				case Namespace: {
					checkAndAddElem<pst::Namespace>(it, ancestors, ancestor, ret, ctx);
					break;
				}
				case Class: {
					checkAndAddElem<pst::Class>(it, ancestors, ancestor, ret, ctx);
					break;
				}
				case Fun: {
					checkAndAddElem<pst::Fun>(it, ancestors, ancestor, ret, ctx);
					break;
				}
				case TemplateStmt: {
					const auto template_stmt_v = ancestor.dynamicCast<pst::TemplateStmt>().value();

					ret += identifier(template_stmt_v->getInnerStatement()
					                      .unlock(ctx)
					                      ->getDeclSymbolIdentifier()
					                      ->unlock(ctx)
					                      ->unwrap()
					                      .strView());

					if (template_stmt_v->hasAdditionalRootData()) {
						// We are inside a baked template

						const auto root_data = template_stmt_v->getAdditionalRootData();
						variant_match(root_data.pst_parent) {
							variant_case(
								pst::AdditionalRootData::BakedTemplateParent, template_parent
							) {
								const auto& template_bake_data_any
									= template_parent.template_bake_data;
								auto template_bake_data
									= base::anyCast<templates::TemplateBakePSTLinkedData>(
										template_bake_data_any
									);

								ret += "I";
								for (const auto& bake_argument: template_bake_data.postponed_data
								                                    ->load(std::memory_order_acquire)
								                                    ->template_arguments_symbols) {
									auto value = ctx.query<helios::QueryConstValueOf>(bake_argument)
									                 .valueOrThrow();
									ret += mangleCTV(ctx, value);
								}
								ret += "E";
							}
							variant_default {
								CORE_PANIC(
									"TemplateStmt has no PST parent, this should not happen "
									"here."
								);
							}
						}
					} else {
						CORE_PANIC(
							"TemplateStmt has no additional root data meaning it is not baked"
							"It should not happen in mangling"
						);
					}
					break;
				}
				default:
					CORE_UNREACHABLE();
				}
			}

			if (!is_template) ret += unscopedName(ctx, symbol_id);

			if (is_nested) ret += "E";

			return ret;
		}

		/**
		 * @brief Returns the symbol's 'path' i.e. in which package/module/script it is defined
		 * and all its enclosing scopes (namespaces, classes, functions, etc.)
		 * @note: See mangling-scheme.md for details
		 */
		std::string path(query::Context& ctx, SymID symbol_id) {
			auto prefix = pathPrefix(ctx, symbol_id);
			// @todo: #3285 add backreferences
			return base::strConcat(std::move(prefix), symbolName(ctx, symbol_id));
		}

		/**
		 * @brief Returns mangled name of a function or method
		 * @note: See mangling-scheme.md for details
		 */
		std::string func(query::Context& ctx, SymID symbol_id) {
			std::stringstream ret;
			if (isFunctionLike(kind(symbol_id))) {
				// @TODO: #2255 Function qualifiers?

				auto function_type = ctx.query<QueryTypeOfSymbol>(symbol_id)->valueOrThrow();
				ret << ctx.query<QueryMangledType>({ function_type })->valueOrThrow().str();

				const auto& fun_decl
					= ctx.query<compiler::helios::QueryDeclOfFun>(symbol_id).get()->valueOrPanic();

				for (const auto& param: fun_decl.parameters) identifier(ret, param.name.strView());

				ret << "E";
			} else {
				CORE_USER_LOG("Tried to mangle non function-like symbol as a function-like.");
				CORE_UNREACHABLE();
			}

			return ret.str();
		}

		/**
		 * @brief Determines what type of symbol we are mangling to choose the right encoding
		 * @note: See mangling-scheme.md for details
		 */
		std::string symbolEncoding(query::Context& ctx, const SymID& symbol_id) {
			switch (kind(symbol_id)) {
			case SymbolKind::Variable:
			case SymbolKind::Field:
			case SymbolKind::Const: {
				// The empty storage of a REPL global variable stands for the variable itself, so
				// everything referring to the variable has to refer to that storage.
				if (auto empty_variable
				    = std::get_if<defgen::ReplEmptyVariable>(&getSymRef(symbol_id)->other))
					return symbolEncoding(ctx, empty_variable->original_variable);

				return path(ctx, symbol_id);
			}

			case SymbolKind::Function:
			case SymbolKind::Method:
			case SymbolKind::Constructor:
			case SymbolKind::Destructor:
			case SymbolKind::FunctionDeclaration: {
				variant_match(getSymRef(symbol_id)->other) {
					variant_case_novalue(PstImplementedSemantics, BuiltinSemantics) {
						// If the symbol originates from the PST (including builtins), use its path.
						return path(ctx, symbol_id) + func(ctx, symbol_id);
					}
					// If the symbol is generated, it has no path.
					variant_case(defgen::Constructor, ctor) {
						// The mangled type identifies which type the constructor belongs to (works
						// for any kind: class, tuple, static array, ...). The infix tag
						// distinguishes the implicit, default and copy constructors.
						const auto mangled_type
							= ctx.query<QueryMangledType>(tsh::SymbolType<>::withDefaults(ctor.type))
						          ->valueOrThrow()
						          .str();

						const char* ctor_tag = [&]() -> const char* {
							switch (ctor.kind) {
							case defgen::Constructor::Kind::Implicit:
								return "Hic";
							case defgen::Constructor::Kind::Default:
								return "Hdc";
							case defgen::Constructor::Kind::Copy:
								return "Hcc";
							}
							CORE_UNREACHABLE();
						}();

						return mangled_type + ctor_tag + func(ctx, symbol_id) + "E";
					}
					variant_case(defgen::Method, method) {
						// We do not have a reliable "path to type" in these cases (esp. for simple
						// types such as i32), so we omit it. Any ambiguities are solved by the
						// function type anyway.
						switch (method.kind) {
						case defgen::Method::Kind::DefaultDestructor:
							return "Hdd" + func(ctx, symbol_id) + "E";
						case defgen::Method::Kind::ToString:
							return "HtoString" + func(ctx, symbol_id) + "E";
						case defgen::Method::Kind::LengthMethod:
							return "Hlength" + func(ctx, symbol_id) + "E";
						}
						CORE_UNREACHABLE();
					}
					variant_case(defgen::BuiltinTemplatedSymbol, builtin) {
						// We do not have a reliable "path to type" in these cases (esp. for simple
						// types such as i32), so we omit it. Any ambiguities are solved by the
						// function type anyway.
						switch (builtin.kind) {
						case defgen::BuiltinTemplatedSymbol::Kind::MoveIn:
							return "Hmin" + func(ctx, symbol_id) + "E";
						}
						CORE_UNREACHABLE();
					}
					variant_case(defgen::ReplInputWrapper, repl_wrapper) {
						return base::strConcat("__repl_input_wrapper_", repl_wrapper.counter);
					}
					// Other cases of generated symbols cannot be functions.
				}
				CORE_UNREACHABLE();
			}
			case SymbolKind::Class: {
				return "C" + path(ctx, symbol_id);
			}
			default:
				throw base::LogicError{
					base::strConcat("Cannot mangle symbol of type: ", kind(symbol_id))
				};
			}
		}

		template<ManglingSymbolKind Kind, class SpecialSymbolKey>
		std::string specialSymbolEncoding(query::Context& ctx, SpecialSymbolKey key);

		template<>
		std::string specialSymbolEncoding<
			ManglingSymbolKind::ModuleConstructor,
			special_symbol_keys::LIRModuleID>(
			query::Context&, special_symbol_keys::LIRModuleID module_id
		) {
			// <encoding> ::= <path>
			// <path> ::= <path-prefix> <symbol-name>
			auto path_prefix = pathPrefix(module_id);

			// <symbol-name> ::= "G" <unscoped-name>
			// <unscoped-name> ::= <special-symbol-encoding>
			// <special-symbol-encoding> ::= "H" <special-symbol-name> "E"
			// <special-symbol-name> ::= "mc"
			auto symbol_name = "GHmcE";

			return base::strConcat(path_prefix, symbol_name);
		}

		template<>
		std::string specialSymbolEncoding<
			ManglingSymbolKind::ModuleDestructor,
			special_symbol_keys::LIRModuleID>(
			query::Context&, special_symbol_keys::LIRModuleID module_id
		) {
			auto path_prefix = pathPrefix(module_id);
			return base::strConcat(path_prefix, "GHmdE");
		}

		template<>
		std::string specialSymbolEncoding<ManglingSymbolKind::GlobalVariableConstructor>(
			query::Context& ctx, SymID symbol_id
		) {
			return symbolEncoding(ctx, symbol_id) + "gc";
		}

		template<>
		std::string specialSymbolEncoding<ManglingSymbolKind::GlobalVariableDestructor>(
			query::Context& ctx, SymID symbol_id
		) {
			return symbolEncoding(ctx, symbol_id) + "gd";
		}

		/**
		 * @brief Get the encoding of a symbol
		 * @note: See mangling-scheme.md for details
		 */
		std::string encoding(query::Context& ctx, const KeyOf_MangledSymbol& key) {
			switch (key.kind) {
			case ManglingSymbolKind::Standard:
				return internal::symbolEncoding(ctx, std::get<SymID>(key.symbol_key));
				break;

			case ManglingSymbolKind::ModuleConstructor:
				return internal::specialSymbolEncoding<ManglingSymbolKind::ModuleConstructor>(
					ctx, std::get<special_symbol_keys::LIRModuleID>(key.symbol_key)
				);
				break;

			case ManglingSymbolKind::ModuleDestructor:
				return internal::specialSymbolEncoding<ManglingSymbolKind::ModuleDestructor>(
					ctx, std::get<special_symbol_keys::LIRModuleID>(key.symbol_key)
				);
				break;

			case ManglingSymbolKind::GlobalVariableConstructor:
				return internal::specialSymbolEncoding<ManglingSymbolKind::GlobalVariableConstructor>(
					ctx, std::get<SymID>(key.symbol_key)
				);
				break;

			case ManglingSymbolKind::GlobalVariableDestructor:
				return internal::specialSymbolEncoding<ManglingSymbolKind::GlobalVariableDestructor>(
					ctx, std::get<SymID>(key.symbol_key)
				);
				break;

			default:
				CORE_PANIC("Unknown ManglingSymbolKind in encoding()");
			}
		}

		/**
		 * @brief Returns formatted metadata that will be added to the mangled name
		 * @note: See mangling-scheme.md for details
		 */
		std::string optMetadata(base::Optional<std::string> metadata) {
			if (!metadata.has_value()) return "";
			return "$" + metadata.value();
		}

		template<typename Float>
		static auto floatToHex(Float value) {
			static constexpr auto HEX_DIGITS = "0123456789abcdef"sv;

			auto bytes = std::bit_cast<std::array<unsigned char, sizeof(Float)>>(value);
			if constexpr (std::endian::native == std::endian::big) std::ranges::reverse(bytes);

			std::string hex_str;
			hex_str.reserve(bytes.size() * 2);

			for (const auto byte: bytes) {
				hex_str += HEX_DIGITS.at(byte >> 4);
				hex_str += HEX_DIGITS.at(byte & 0x0F);
			}

			return hex_str;
		}

		static std::string mangleValue(query::Context&, compiler::numeric_value::NumericValue num) {
			const auto& value = num.getStorage();

			variant_match(value) {
				variant_case_novalue(int8_t, i16, i32, i64) {
					const i64 int_value = std::visit(
						[&](auto&& arg) -> i64 { return static_cast<i64>(arg); }, value
					);
					const u64 magnitude = int_value < 0 ? 0u - static_cast<u64>(int_value)
					                                    : static_cast<u64>(int_value);
					return base::strConcat((int_value < 0 ? "n" : ""), magnitude, "_");
				}
				variant_case_novalue(uint8_t, u16, u32, u64) {
					const u64 uint_value = std::visit(
						[&](auto&& arg) -> u64 { return static_cast<u64>(arg); }, value
					);
					return base::strConcat(uint_value, "_");
				}
				variant_case(f32, f) { return floatToHex(f); }
				variant_case(f64, d) { return floatToHex(d); }
				variant_default { CORE_PANIC("Unknown type in mangleValue()"); }
			}
		}

		std::string mangleString(std::string_view sv) {
			static constexpr auto HEX_DIGITS = "0123456789abcdef"sv;
			std::string           hex_str;
			hex_str.reserve(sv.size() * 2);

			for (const char c: sv) {
				const auto byte = static_cast<unsigned char>(c);
				hex_str += HEX_DIGITS.at(byte >> 4);
				hex_str += HEX_DIGITS.at(byte & 0x0F);
			}

			return base::strConcat(sv.size(), '_', hex_str);
		}

	}  // namespace internal

	std::string mangleCTV(query::Context& ctx, const compiler::ctv::CompileTimeValue& value) {
		// @todo: #3285 add backreferences
		variant_match(value.getStorage()) {
			variant_case(bool, b) { return base::strConcat("b", (b ? "1" : "0")); }
			variant_case(compiler::numeric_value::NumericValue, num) {
				return base::strConcat(
					ctx.query<QueryMangledType>(num.getTypeOfStoredValue(ctx))
						.get()
						->valueOrThrow()
						.strView(),
					internal::mangleValue(ctx, num)
				);  // adds '_' to ints
			}
			variant_case(char, c) {
				return base::strConcat("c", static_cast<u32>(static_cast<unsigned char>(c)), "_");
			}
			variant_case(compiler::ctv::CompileTimeValue::CharSliceValue, cs) {
				return base::strConcat("r", internal::mangleString(cs.value.strView()));
			}
			variant_case(compiler::ctv::CompileTimeValue::StringClassValue, sc) {
				return base::strConcat("s", internal::mangleString(sc.value.strView()));
			}
			variant_case(compiler::ctv::CompileTimeValue::UnitCTV, unit) { return "u"; }
			variant_case(compiler::ctv::CompileTimeValue::TupleCTV, tuple) {
				std::string ret = "T";
				for (auto&& elem: tuple.getElements()) ret += mangleCTV(ctx, elem);
				return ret += "E";
			}
			variant_case(tsh::SymbolType<>, sym) {
				return "t" + ctx.query<QueryMangledType>({ sym })->valueOrThrow().str();
			}
			variant_default { CORE_PANIC("Unknown CTV type in mangle(CTV)"); }
		}
	}

	struct IMPLEMENT_QUERY(QueryMangledSymbol, base::StrID) {
		static auto provide(Context& ctx, const QKey& key) -> PResult {
			if (not internal::shouldMangle(ctx, key)) return name(std::get<SymID>(key.symbol_key));

			// note: global identifiers starting with underscore and a capital letter are
			// reserved in C. Q seems to be free and stands for both query and quack
			constexpr auto LANGUAGE_PREFIX = "_Q"sv;

			const auto mangling_scheme_version
				= internal::compactNumber(key.mangling_scheme_version);

			const std::string encoding = internal::encoding(ctx, key);

			const std::string metadata = internal::optMetadata(key.additional_metadata);

			std::string mangled_name
				= base::strConcat(LANGUAGE_PREFIX, mangling_scheme_version, encoding, metadata);

			return base::StrID{ mangled_name.c_str() };
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryMangledSymbol);

	struct IMPLEMENT_QUERY(QueryMangledType, query::QResult<base::StrID>) {
		static std::string mangle(query::Context&, tsh::UnitAbstractType) { return "u"; }

		static std::string mangle(query::Context&, tsh::VoidAbstractType) { return "v"; }

		static std::string mangle(query::Context&, tsh::ByteAbstractType) { return "y"; }

		static std::string mangle(query::Context&, tsh::BoolAbstractType) { return "b"; }

		static std::string mangle(query::Context&, tsh::CharAbstractType) { return "c"; }

		static std::string mangle(query::Context&, tsh::IntegralAbstractType type) {
			// note: new integers' names are not updated in the scheme
			// it will be done in #3417
			const bool is_signed
				= type.getSignedness() == tsh::IntegralAbstractType::Signedness::Signed;
			const auto  size = type.getSize().asInt();
			std::string ret  = (is_signed ? "i" : "j");
			switch (size) {
			case 8:
				ret += "b";
				break;
			case 16:
				ret += "w";
				break;
			case 32:
				ret += "d";
				break;
			case 64:
				ret += "q";
				break;
			case 128:
				ret += "x";
				break;
			default:
				ret = (is_signed ? "k" : "l") + std::to_string(size) + "_";
			}
			return ret;
		}

		static std::string mangle(query::Context&, tsh::FloatAbstractType type) {
			const auto size = type.getSize().asInt();
			switch (size) {
			case 16:
				return "h";
			case 32:
				return "f";
			case 64:
				return "d";
			case 80:
				return "e";
			case 128:
				return "q";
			default:
				// @future: brain float
				CORE_PANIC("Unknown floating-point type in mangle(FloatAbstractType)");
			}
		}

		static std::string mangle(query::Context&, tsh::RawPointerAbstractType) { return "p"; }

		static std::string mangle(query::Context& ctx, tsh::PointerAbstractType type) {
			return base::strConcat(
				"P", ctx.query<QueryMangledType>({ type.getPointee() })->valueOrThrow().str(), "E"
			);
		}

		static std::string mangle(query::Context& ctx, tsh::ManyPointerAbstractType type) {
			return base::strConcat(
				"MP", ctx.query<QueryMangledType>({ type.getPointee() })->valueOrThrow().str(), "E"
			);
		}

		static std::string mangle(query::Context& ctx, tsh::CPointerAbstractType type) {
			return base::strConcat(
				"CP", ctx.query<QueryMangledType>({ type.getPointee() })->valueOrThrow().str(), "E"
			);
		}

		static std::string mangle(query::Context& ctx, tsh::SliceAbstractType type) {
			return base::strConcat(
				"S",
				ctx.query<QueryMangledType>({ type.getElementType() })->valueOrThrow().str(),
				"E"
			);
		}

		static std::string mangle(query::Context& ctx, tsh::FunctionAbstractType type) {
			std::stringstream res;
			res << "F"
				<< ctx.query<QueryMangledType>({ type.getResultType() })->valueOrThrow().str();
			// @TODO: #2255 Function qualifiers?
			for (auto& param: type.getParameterTypes())
				res << ctx.query<QueryMangledType>({ param })->valueOrThrow().str();
			res << "E";

			return res.str();
		}

		static std::string mangle(query::Context& ctx, tsh::StaticArrayAbstractType type) {
			return base::strConcat(
				"A",
				base::toString(type.getSize()),
				ctx.query<QueryMangledType>({ type.getElementType() })->valueOrThrow().str(),
				"E"
			);
		}

		static std::string mangle(query::Context& ctx, tsh::TupleAbstractType type) {
			std::stringstream res;
			res << "T";
			for (auto& elem: type.getComponents())
				res << ctx.query<QueryMangledType>({ elem })->valueOrThrow().str();
			res << "E";

			return res.str();
		}

		static std::string mangle(query::Context& ctx, tsh::VariantAbstractType type) {
			std::stringstream res;
			res << "V";
			for (auto& elem: type.getUnderlyingTypes())
				res << ctx.query<QueryMangledType>({ elem })->valueOrThrow().str();
			res << "E";

			return res.str();
		}

		static std::string mangle(query::Context& ctx, tsh::ClassAbstractType type) {
			return getSimpleMangledName(ctx, type.getSymbol()).str();
		}

		static std::string mangle(query::Context&, tsh::MetaAbstractType) { return "t"; }

		static query::QResult<std::string> mangle(query::Context& ctx, tsh::AbstractType type) {
			using enum tsh::Kind;
			switch (type.getKind()) {
			case Unit:
				return mangle(ctx, type.as<tsh::UnitAbstractType>());
			case Void:
				return mangle(ctx, type.as<tsh::VoidAbstractType>());
			case Byte:
				return mangle(ctx, type.as<tsh::ByteAbstractType>());
			case Bool:
				return mangle(ctx, type.as<tsh::BoolAbstractType>());
			case Char:
				return mangle(ctx, type.as<tsh::CharAbstractType>());
			case Integral:
				return mangle(ctx, type.as<tsh::IntegralAbstractType>());
			case Float:
				return mangle(ctx, type.as<tsh::FloatAbstractType>());
			case RawPointer:
				return mangle(ctx, type.as<tsh::RawPointerAbstractType>());
			case Pointer:
				return mangle(ctx, type.as<tsh::PointerAbstractType>());
			case ManyPointer:
				return mangle(ctx, type.as<tsh::ManyPointerAbstractType>());
			case CPointer:
				return mangle(ctx, type.as<tsh::CPointerAbstractType>());
			case Slice:
				return mangle(ctx, type.as<tsh::SliceAbstractType>());
			case Function:
				return mangle(ctx, type.as<tsh::FunctionAbstractType>());
			case StaticArray:
				return mangle(ctx, type.as<tsh::StaticArrayAbstractType>());
			case Tuple:
				return mangle(ctx, type.as<tsh::TupleAbstractType>());
			case Variant:
				return mangle(ctx, type.as<tsh::VariantAbstractType>());
			case Class:
				return mangle(ctx, type.as<tsh::ClassAbstractType>());
			case Meta:
				return mangle(ctx, type.as<tsh::MetaAbstractType>());
			default:
				ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
					base::strConcat("Cannot mangle type of kind: ", type.getKind()), ""
				));
				return query::Failed();
			}
		}

		static auto provide(Context& ctx, const QKey& key) -> PResult {
			// @todo: #3285 add backreferences
			return base::StrID{ base::strConcat(
				key.getUniqueness() == tsh::Uniqueness::Unique ? "M" : "",
				key.getLeakage() == tsh::Leakage::Leaking ? "L" : "",
				key.getMutability() == tsh::Mutability::Mutable ? "" : "N",
				key.getRefKind() == tsh::ReferenceKind::Direct ? ""
				: key.getRefKind() == tsh::ReferenceKind::Box  ? "X"
															   : "R",
				mangle(ctx, key.getType()).valueOrThrow()
			) };
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryMangledType);

	base::StrID getSimpleMangledName(query::Context& ctx, SymID sym_id) {
		return ctx.query<QueryMangledSymbol>(KeyOf_MangledSymbol{ .symbol_key = sym_id });
	}

	template<>
	base::StrID getSpecialMangledName<
		ManglingSymbolKind::ModuleConstructor,
		special_symbol_keys::LIRModuleID>(
		query::Context& ctx, special_symbol_keys::LIRModuleID mod_id
	) {
		return ctx.query<QueryMangledSymbol>(KeyOf_MangledSymbol{
			.symbol_key = mod_id, .kind = ManglingSymbolKind::ModuleConstructor });
	}

	template<>
	base::StrID getSpecialMangledName<
		ManglingSymbolKind::ModuleDestructor,
		special_symbol_keys::LIRModuleID>(
		query::Context& ctx, special_symbol_keys::LIRModuleID mod_id
	) {
		return ctx.query<QueryMangledSymbol>(KeyOf_MangledSymbol{
			.symbol_key = mod_id, .kind = ManglingSymbolKind::ModuleDestructor });
	}

	template<>
	base::StrID getSpecialMangledName<ManglingSymbolKind::GlobalVariableConstructor>(
		query::Context& ctx, SymID sym_id
	) {
		return ctx.query<QueryMangledSymbol>(KeyOf_MangledSymbol{
			.symbol_key = sym_id, .kind = ManglingSymbolKind::GlobalVariableConstructor });
	}

	template<>
	base::StrID getSpecialMangledName<ManglingSymbolKind::GlobalVariableDestructor>(
		query::Context& ctx, SymID sym_id
	) {
		return ctx.query<QueryMangledSymbol>(KeyOf_MangledSymbol{
			.symbol_key = sym_id, .kind = ManglingSymbolKind::GlobalVariableDestructor });
	}
}
