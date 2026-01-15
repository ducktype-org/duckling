#include "mangler.hpp"

#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/element_kind.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries.hpp>
#include <helios/scope_symbol_id.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/simple.hpp>
#include <helios/symbols/symbol_abi.hpp>
#include <helios/utils/go_to_definition.hpp>
#include <helios_private/expressions/query_hout_of_expr.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <typesystem/higher/types.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <query_framework/query_impl.hpp>

#include <algorithm>
#include <string_view>

/**
 * This is the implementation of the mangling scheme according to mangling-scheme.md
 * If any changes are made to this file, please also update the mangling-scheme.md
 * That file provides a detailed description and motivation for some design choices made here
 */
namespace compiler::helios::mangler {

	constexpr auto KeyOf_MangledSymbol::operator<=>(const KeyOf_MangledSymbol& other) const {
		return std::tie(symbol_key, kind, mangling_scheme_version, additional_metadata)
		   <=> std::tie(
				   other.symbol_key,
				   other.kind,
				   other.mangling_scheme_version,
				   other.additional_metadata
		   );
	}

	u64 KeyOf_MangledSymbol::queryUnstablePerfectHash() const {
		static base::Map<KeyOf_MangledSymbol, u64> hashes{};

		if (const auto iter = hashes.find(*this); iter != hashes.end()) return iter->second;

		u64 result = hashes.size();
		hashes.put(*this, result);
		return result;
	}

	namespace internal {
		/**
		 * @brief A shorter representation of a number in base-62, used to save space
		 * @note: See mangling-scheme.md for details
		 */
		std::string compactNumber(u64 number) {
			using namespace std::literals::string_view_literals;

			if (number == 0) return "_";

			static constexpr auto DIGITS
				= "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"sv;
			constexpr u64 BASE = DIGITS.size();

			std::string ret;
			--number;
			do {
				ret += DIGITS[number % BASE];
				number /= BASE;
			} while (number > 0);
			std::ranges::reverse(ret);
			ret += '_';
			return ret;
		}

		/**
		 * @brief Returns bare identifier of the symbol prefixed with its size
		 * If the name contains characters outside of the allowed set,
		 * it will be prefixed with 'U' and punnycode-encoded.
		 * @note: See mangling-scheme.md for details
		 */
		std::string identifier(std::string name) {
			// @future: use punnycode for unicode strings
			if (/* hasCharsOnlyFromAllowedCharacterSet */ true) {
				name = std::to_string(name.size()) + name;
				return name;
			} else {
				constexpr char UNICODE_PREFIX = 'U';
				std::string    punny_string   = name;  // @future: convert to punnycode
				return base::strConcat(UNICODE_PREFIX, punny_string.size(), punny_string);
			}
		}

		/**
		 * @brief Returns package/module/script prefix for the symbol
		 * @note: See mangling-scheme.md for details
		 */
		std::string pathPrefix(SymID symbol_id) {
			auto enclosing_scope  = scope(symbol_id);
			auto enclosing_module = module(enclosing_scope);

			// note: only the enclosing module is used for mangling. This is intentional,
			// as modules are supposed to be self-contained and this would make moving them
			// a more breaking (ABI-wise) change than it should be

			// "M" <module-name>                                 // standalone module
			// @future: templated modules
			if (/* standalone module */ true) {
				auto module_identifier = identifier(frontend::moduleName(enclosing_module).str());
				return base::strConcat("M", module_identifier);
			}

			// @future: add support for packages & scripts when they are implemented
			// "P" <package-name> <module-name>                  // module in a package
			// "S" <script-name>                                 // standalone script
			// "R" <package-name> <module-name> <script-name>    // script in a package

			// @todo: backreference -- this will be added in the next PR
		}

		std::string pathPrefix(special_symbol_keys::LIRModuleID mod_id) {
			// "M" <module-name>                                 // standalone module
			return base::strConcat("M", mod_id.id);

			// @future: add support for packages & scripts when they are implemented
		}

		/**
		 * @brief Returns the bare name of the symbol prefixed with its size
		 * @note: See mangling-scheme.md for details
		 */
		std::string unscopedName(SymID symbol_id) {
			return identifier(compiler::helios::name(symbol_id).str());
		}

		/**
		 * @brief Returns symbol name prefixed with all enclosing it scopes to uniquely identify it
		 * @note: See mangling-scheme.md for details
		 *
		 * @todo: This is still a little simplified, there should probably be at least an additional
		 * layer for things like macros and there will probably be other elements that create scopes.
		 */
		std::string symbolName(query::Context& ctx, SymID symbol_id, bool add_symbol_name = true) {
			auto scope_id = scope(symbol_id);

			if (scopeDepth(scope_id) == 1) {
				return "G" + unscopedName(symbol_id);
			} else {
				std::vector<std::string> path_parts;

				auto current_pst = symbolPst(symbol_id).unlock(ctx);
				while (true) {
					auto ancestor     = current_pst;
					auto ancestor_opt = ancestor->getParent();

					while (ancestor_opt) {
						ancestor = ancestor_opt.value().unlock(ctx);

						if (ancestor->getElementKind() == pst::ElementKind::Namespace) {
							auto nmsp = ancestor.dynamicCast<pst::Namespace>().value();
							path_parts.push_back(identifier(nmsp->getName().str()));
							current_pst = pst::Access<pst::LangElement>(ancestor);
							break;
						}
						if (ancestor->getElementKind() == pst::ElementKind::Class) {
							auto nmsp = ancestor.dynamicCast<pst::Class>().value();
							path_parts.push_back(identifier(nmsp->getName().str()));
							current_pst = pst::Access<pst::LangElement>(ancestor);
							break;
						}

						ancestor_opt = ancestor->getParent();
					}

					if (!ancestor_opt) break;
				}

				std::string ret = "N";
				for (auto&& it = path_parts.rbegin(); it != path_parts.rend(); ++it) ret += *it;

				if (add_symbol_name) ret += unscopedName(symbol_id);

				return ret + "E";
			}
		}

		/**
		 * @brief Returns the symbol's 'path' i.e. in which package/module/script it is defined
		 * and all its enclosing scopes (namespaces, classes, functions, etc.)
		 * @note: See mangling-scheme.md for details
		 */
		std::string path(query::Context& ctx, SymID symbol_id, bool add_symbol_name = true) {
			return base::strConcat(
				pathPrefix(symbol_id), symbolName(ctx, symbol_id, add_symbol_name)
			);
		}

		/**
		 * @brief Returns mangled name of a function or method
		 * @note: See mangling-scheme.md for details
		 */
		std::string funcType(query::Context& ctx, SymID symbol_id) {
			// @TODO: #1568 use type mangling for parameter and return types.
			std::string ret;
			if (kind(symbol_id) == SymbolKind::Function
			    or kind(symbol_id) == SymbolKind::FunctionDeclaration) {
				ret = "F";

				const auto& fun_decl
					= ctx.query<compiler::helios::QueryDeclOfFun>(symbol_id).get()->valueOrPanic();
				ret += fun_decl.return_type.toString();

				for (const auto& param: fun_decl.parameters) {
					ret += param.type.toString();
					ret += identifier(param.name.str());
				}

				ret += "E";
			} else if (kind(symbol_id) == SymbolKind::Method) {
				// @future: add methods when they are implemented
				ret = "Ftodo_method_typeE";
			}

			return ret;
		}

		/**
		 * @brief Returns mangled name of a special member (ctor, dtor, etc.)
		 * @note: See mangling-scheme.md for details
		 */
		std::string specialMemberType([[maybe_unused]] query::Context& ctx, SymID symbol_id) {
			auto        kind = compiler::helios::kind(symbol_id);
			std::string ret;

			if (kind == SymbolKind::Constructor) {
				ret = "C";

				auto pst = symbolPst(symbol_id).unlock(ctx);
				for (const pst::LangElement::Child& child_locked: pst->viewChildren()) {
					auto child = child_locked.unlock(ctx);
					if (child->getElementKind() == pst::ElementKind::ParamList) {
						pst::Access<pst::ParamList> list
							= child.dynamicCast<pst::ParamList>().value();
						for (auto param: std::ranges::subrange(list->begin(), list->end())) {
							auto expr = param.unlock(ctx)->getType().unlock(ctx)->getExpr();

							compiler::helios::ExprConstructionResult hout_expr
								= ctx.query<compiler::helios::QueryHoutOfExpr>(expr);

							auto type = hout_expr.valueOrThrow()
							                ->expression_type.getSymbolType()
							                .getType()
							                .toString();  // "META" ?

							// auto type = compiler::helios::querySymIDOfPSTExpr(ctx, expr); // empty

							// auto symid = querySymIDOfHOUTExpr(ctx, hout_expr.value().ref()); //
							// empty auto type = ctx.query<QueryTypeOfSymbol>({ symid });

							ret += identifier(type);
						}
						break;
					}
				}

				ret += "E";
			} else if (kind == SymbolKind::Destructor) {
				ret = "D";

				auto pst = symbolPst(symbol_id).unlock(ctx);
				for (const pst::LangElement::Child& child_locked: pst->viewChildren()) {
					auto child = child_locked.unlock(ctx);
					if (child->getElementKind() == pst::ElementKind::ParamList) {
						pst::Access<pst::ParamList> list
							= child.dynamicCast<pst::ParamList>().value();
						for (auto param: std::ranges::subrange(list->begin(), list->end())) {
							auto expr = param.unlock(ctx)->getType().unlock(ctx)->getExpr();

							compiler::helios::ExprConstructionResult hout_expr
								= ctx.query<compiler::helios::QueryHoutOfExpr>(expr);

							auto type = hout_expr.valueOrThrow()
							                ->expression_type.getSymbolType()
							                .getType()
							                .toString();

							ret += identifier(type);
						}
						break;
					}
				}

				ret += "E";
			} else {
				// @future: implement mangling for other special members
				ret = "Mangling_of_this_special_member_is_not_implemented_yet";
				throw base::NotYetImplemented(
					"Mangling of this special member is not implemented yet"
				);
			}

			return ret;
		}

		/**
		 * @brief Determines what type of symbol we are mangling to choose the right encoding
		 * @note: See mangling-scheme.md for details
		 */
		std::string symbolEncoding(query::Context& ctx, const SymID& symbol_id) {
			switch (kind(symbol_id)) {
			case SymbolKind::Variable:
			case SymbolKind::Field:
			case SymbolKind::Const:
				return path(ctx, symbol_id);
				break;

			case SymbolKind::Function:
			case SymbolKind::Method:
			case SymbolKind::FunctionDeclaration: {
				variant_match(getSymRef(symbol_id)->other) {
					variant_case_novalue(PstSymbolData) {
						// If the symbol originates from the PST, use its path.
						return path(ctx, symbol_id) + funcType(ctx, symbol_id);
					}
					variant_case(houtgen::GeneratedSymbolData, gen_data) {
						// If the symbol is generated, it has no path.
						variant_match(gen_data.data) {
							variant_case(houtgen::GeneratedSymbolData::ImplicitConstructor, ctor) {
								const auto path_to_class = path(ctx, ctor.class_symbol);
								const auto ctor_suffix   = "C" + funcType(ctx, symbol_id) + "E";
								return path_to_class + ctor_suffix;
							}
							variant_case(
								houtgen::GeneratedSymbolData::ReplExpressionWrapper, repl_wrapper
							) {
								return base::strConcat("__repl_expr_wrapper_", repl_wrapper.counter);
							}
							// Other cases of generated symbols cannot be functions.
						}
					}
					// The last case is that the symbol is a builtin function, which is handled
					// in a separate branch of ImplementationOf_QueryMangledSymbol::provide.
					// @TODO: #1700 Simplify this handling of builtin functions.
				}
				CORE_UNREACHABLE();
			}
			case SymbolKind::Class: {
				// @TODO: #1568 generalise type mangling?
				return path(ctx, symbol_id);
			}

				// @note Currently, we are handling constructor mangling differently (as functions).
				// However, this code existed earlier and provided a different mangling scheme
				// for constructors and destructors, which we may want to reference in the future.
				//
				// case SymbolKind::Constructor:
				// case SymbolKind::Destructor:
				// 	return path(ctx, symbol_id, false) + specialMemberType(ctx, symbol_id);
				// 	break;

			default:
				throw base::LogicError{ base::strConcat(
					"Cannot mangle symbol of type: ", symbolPst(symbol_id).unlock(ctx)->elementType()
				) };
				break;
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
		 * @brief Returns formatted metadata that will be added to the mangled name
		 * @note: See mangling-scheme.md for details
		 */
		std::string optMetadata(base::Optional<std::string> metadata) {
			if (!metadata.has_value()) return "";
			return "$" + metadata.value();
		}

	}  // namespace internal

	struct IMPLEMENT_QUERY(QueryMangledSymbol, base::StrID) {
		static auto provide(Context& ctx, const QKey& key) -> PResult {
			using namespace std::literals::string_view_literals;

			if (key.kind == ManglingSymbolKind::Standard) {
				auto sym_id = std::get<SymID>(key.symbol_key);
				// a temporary hack:
				// @TODO: #895 fix it when we add script based package targets
				if (name(sym_id) == "main") {
					// main is not mangled
					return base::StrID{ "main" };
				}

				if (std::holds_alternative<builtin::BuiltinFunctionData>(getSymRef(sym_id)->other)) {
					// Builtin functions are not mangled
					// @TODO: #1700 Simplify this handling of builtin functions.
					// i.e. probably make it similar to mangling regular functions.
					return name(sym_id);
				}

				if (auto abi = ctx.query<QuerySymbolABI>(sym_id); abi->hasValue()) {
					variant_match(abi->valueOrThrow()) {
						variant_case_novalue(CAbi) { return name(sym_id); }
						variant_case_novalue(DefaultAbi) { /* Handled below */ }
						variant_default { CORE_UNREACHABLE(); }
					}
				}
			}

			// note: global identifiers starting with underscore and a capital letter are
			// reserved in C. Q seems to be free and stands for both query and quack
			constexpr auto LANGUAGE_PREFIX = "_Q"sv;

			const auto mangling_scheme_version
				= internal::compactNumber(key.mangling_scheme_version);

			const std::string encoding = [&key, &ctx] {
				switch (key.kind) {
				case ManglingSymbolKind::Standard:
					return internal::symbolEncoding(ctx, std::get<0>(key.symbol_key));
					break;

				case ManglingSymbolKind::ModuleConstructor:
					return internal::specialSymbolEncoding<ManglingSymbolKind::ModuleConstructor>(
						ctx, std::get<1>(key.symbol_key)
					);
					break;

				case ManglingSymbolKind::ModuleDestructor:
					return internal::specialSymbolEncoding<ManglingSymbolKind::ModuleDestructor>(
						ctx, std::get<1>(key.symbol_key)
					);
					break;

				case ManglingSymbolKind::GlobalVariableConstructor:
					return internal::specialSymbolEncoding<
						ManglingSymbolKind::GlobalVariableConstructor>(
						ctx, std::get<0>(key.symbol_key)
					);
					break;

				case ManglingSymbolKind::GlobalVariableDestructor:
					return internal::specialSymbolEncoding<
						ManglingSymbolKind::GlobalVariableDestructor>(
						ctx, std::get<0>(key.symbol_key)
					);
					break;

				default:
					CORE_UNREACHABLE();
				}
			}();

			std::string metadata = internal::optMetadata(key.additional_metadata);

			std::string mangled_name
				= base::strConcat(LANGUAGE_PREFIX, mangling_scheme_version, encoding, metadata);

			return base::StrID{ mangled_name.c_str() };
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryMangledSymbol);

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
