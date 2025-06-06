#include "mangler.hpp"

#include "../../../src_private/helios_private/scopes/scopes.hpp"
#include "../../../src_private/helios_private/symbols/symbol_data.hpp"
#include "../../../src_private/helios_private/symbols/symbols.hpp"

#include <frontend/module_tree/queries.hpp>
#include <helios/scope_symbol_id.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/simple.hpp>
#include <pst_parser/element_kind.hpp>
#include <pst_parser/elements/hierarchy/class_elements/all_class_elements.hpp>
#include <pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <query_framework/query_impl.hpp>

#include <algorithm>
#include <ranges>
#include <string_view>

/**
 * This is the implementation of the mangling scheme according to mangling-scheme.md
 * That file provides a detailed description and motivation for some design choices made here
 * Many functions correspond to the
 */
namespace compiler::helios::mangler {

	constexpr auto KeyOf_MangledSymbol::operator<=>(const KeyOf_MangledSymbol& other) const {
		return std::tie(symbol, mangling_scheme_version, additional_metadata)
		   <=> std::tie(other.symbol, other.mangling_scheme_version, other.additional_metadata);
	}

	u64 KeyOf_MangledSymbol::queryUnstablePerfectHash() const {
		static base::Map<KeyOf_MangledSymbol, u64> hashes{};

		if (const auto iter = hashes.find(*this); iter != hashes.end()) return iter->second;

		u64 result = hashes.size();
		hashes.put(*this, result);
		return result;
	}

	namespace detail {

		/**
		 * @brief Checks if the symbol can be mangled
		 */
		bool isManglable(SymID symbol_id) {
			switch (kind(symbol_id)) {
			case SymbolKind::Variable:
			case SymbolKind::Field:
			case SymbolKind::Const:

			case SymbolKind::Function:
			case SymbolKind::Method:

			case SymbolKind::Constructor:
			case SymbolKind::Destructor:
				return true;
			default:
				return false;
			}
		}

		/**
		 * @brief A shorter representation of a number in base-62, used to save space
		 * @note: See mangling-scheme.md for details
		 */
		std::string compactNumber(u64 number) {
			using namespace std::literals::string_view_literals;

			if (number == 0) return "_";

			static constexpr auto digits
				= "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"sv;
			constexpr u64 base = digits.size();

			std::string ret;
			--number;
			do {
				ret += digits[number % base];
				number /= base;
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
				constexpr char unicode_prefix = 'U';
				std::string    punny_string   = name;  // @future: convert to punnycode
				return base::strConcat(unicode_prefix, punny_string.size(), punny_string);
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

			// @todo: backreference
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
		 */
		std::string symbolName(query::Context& ctx, SymID symbol_id) {
			auto scope_id = scope(symbol_id);

			if (scopeDepth(scope_id) == 1) {
				return "G" + unscopedName(symbol_id);
			} else {
				std::vector<std::string> path_parts;

				auto current_scope  = scope_id;
				auto current_pst_id = symbolPst(symbol_id).unlock(ctx)->getID();
				while (true) {
					auto parent_scope = parent(current_scope);
					if (!parent_scope.has_value()) break;

					auto symbols_in_parent
						= *ctx.query<compiler::helios::QuerySymbolsInScope>(parent_scope.value())
					           .get();
					for (const auto& sym: symbols_in_parent) {
						auto pst_sym = symbolPst(sym).unlock(ctx);

						if (pst_sym->getElementKind() == pst::ElementKind::Namespace) {
							auto nmsp = pst_sym.dynamicCast<pst::Namespace>().value();
							auto body = nmsp->getBody().unlock(ctx);

							for (auto&& child_lck: body->viewChildren()) {
								auto child = child_lck.unlock(ctx);

								if (child->getID() == current_pst_id) {
									path_parts.push_back(identifier(nmsp->getName().str()));
									current_pst_id = pst_sym->getID();
								}
							}
						} else if (pst_sym->getElementKind() == pst::ElementKind::Class) {
							auto cls  = pst_sym.dynamicCast<pst::Class>().value();
							auto body = cls->getBody().unlock(ctx);

							for (auto&& child_lck: body->viewChildren()) {
								auto child = child_lck.unlock(ctx);

								if (child->getID() == current_pst_id) {
									path_parts.push_back(identifier(cls->getName().str()));
									current_pst_id = pst_sym->getID();
								}
							}
						}
						// @future: local classes (mangle enclosing function name)
					}

					current_scope = parent_scope.value();
				}

				std::string ret = "N";
				for (auto&& it = path_parts.rbegin(); it != path_parts.rend(); ++it) ret += *it;
				ret += unscopedName(symbol_id) + "E";
				return ret;
			}
		}

		/**
		 * @brief Returns the symbol's path
		 * @note: See mangling-scheme.md for details
		 */
		std::string path(query::Context& ctx, SymID symbol_id) {
			return base::strConcat(pathPrefix(symbol_id), symbolName(ctx, symbol_id));
		}

		/**
		 * @brief Returns mangled name of a function or method
		 * @note: See mangling-scheme.md for details
		 */
		std::string funcType(query::Context& ctx, SymID symbol_id) {
			std::string ret;
			if (kind(symbol_id) == SymbolKind::Function) {
				ret       = "F";
				auto type = ctx.query<QueryTypeOfSymbol>({ symbol_id }).get()->value().getType();
				auto fun_type = tsh::FunctionAbstractType(type);

				auto ret_type = fun_type.getResultType();
				ret += ret_type.toString();
				for (auto param: fun_type.getParameterTypes()) ret += param.toString();

				ret += "E";
			} else if (kind(symbol_id) == SymbolKind::Method) {
				// @future: add methods when they are implemented
				ret = "Ftodo_method_typeE";
			}

			return ret;
		}

		/**
		 * @brief Determines what tipe of symbol we are mangling to choose the right encoding
		 * @note: See mangling-scheme.md for details
		 */
		std::string symbolEncoding(query::Context& ctx, SymID symbol_id) {
			switch (kind(symbol_id)) {
			case SymbolKind::Variable:
			case SymbolKind::Field:
			case SymbolKind::Const:
				return path(ctx, symbol_id);
				break;

			case SymbolKind::Function:
			case SymbolKind::Method:
				return path(ctx, symbol_id) + funcType(ctx, symbol_id);
				break;

			case SymbolKind::Constructor:
			case SymbolKind::Destructor:
				// special symbols
				return "todo_special_symbols";  // @future
				break;

			default:
				return "todo_unknown_symbol";  // @future
				break;
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

	}  // namespace detail

	struct IMPLEMENT_QUERY(QueryMangledSymbol, base::Optional<std::string>) {
		static auto provide(Context& ctx, const QKey& key) -> PResult {
			using namespace std::literals::string_view_literals;

			if (!detail::isManglable(key.symbol)) return std::nullopt;  // @todo: wrong symbol kind

			// note: global identifiers starting with underscore and a capital letter are reserved
			// in C Q seems to be free and stands for both query and quack
			constexpr auto language_prefix = "_Q"sv;

			const auto mangling_scheme_version = detail::compactNumber(key.mangling_scheme_version);
			std::string encoding               = detail::symbolEncoding(ctx, key.symbol);
			std::string metadata               = detail::optMetadata(key.additional_metadata);

			std::string mangled_name
				= base::strConcat(language_prefix, mangling_scheme_version, encoding, metadata);

			return mangled_name;
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryMangledSymbol);


}  // namespace compiler::helios::mangler
