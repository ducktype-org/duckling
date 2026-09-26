#pragma once

#include "frontend/module_tree/module_id.hpp"
#include "lookup_result.hpp"

#include <frontend/pst_parser/source_position_locked.hpp>
#include <helios/scope_id.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <helios/tsh/abstract_type.hpp>

#include <base/collections/optional.hpp>
#include <base/pointers/box.hpp>

#include <diagnostic/stable_position.hpp>
#include <query_framework/query_result.hpp>

#include <variant>

namespace dia {
	// Forward declaration of SourcePosition
	class SourcePosition;
}

namespace compiler::helios {

	struct AdditionalLookupParameters final {
		bool with_wildcards = true;

		/**
		 * @brief The scope the lookup is written in.
		 *
		 * It decides which private and protected members of a class are visible.
		 */
		base::Optional<ScopeID> accessing_scope{};
	};

	/**
	 * @brief A class that can be used to create custom interfaces through inheritance.
	 */
	class CustomInterfaceABC {
	public:
		virtual ~CustomInterfaceABC() = default;

		/**
		 * @brief Perform a lookup in the custom interface.
		 * @param ctx The context to use for the lookup.
		 * @param name The name to look up.
		 * @param params Additional parameters for the lookup.
		 * @return The result of the lookup.
		 */
		virtual CRef<query::QResult<LookupResult>> lookup(
			query::Context& ctx, base::StrID name, AdditionalLookupParameters params
		) = 0;
	};

	/**
	 * HInterface is an abstraction over the lookup process.
	 * It is used to perform lookups in different contexts, and on different entities.
	 * @note All lookups should be performed through HInterface.
	 */
	class HInterface final {
		/**
		 * @brief The interface of the scope.
		 * It lookups from the set of all the symbols in the scope.
		 */
		struct ScopeInterface {
			helios::ScopeID scope;
		};

		/**
		 * @brief The interface of the scope and all its parents.
		 * It lookups from the set of all the symbols in the scope and all the scopes parents.
		 */
		struct ScopeWithParentsInterface {
			helios::ScopeID scope;
		};

		/**
		 * @brief Interface of a module.
		 */
		struct ModuleInterface {
			helios::SymID id;
		};

		/**
		 * @brief Interface of a namespace.
		 */
		struct NamespaceInterface {
			helios::SymID id;
		};

		/**
		 * @brief Interface of an import.
		 */
		struct ImportInterface {
			helios::SymID symbol;
		};

		/**
		 * @brief Interface of a using.
		 */
		struct UsingInterface {
			helios::SymID symbol;
		};

		/**
		 * @brief The interface of the type instance.
		 * The behavior depends on the type, but in general it
		 * represents what `symbol-of-given-type.abc` would do.
		 */
		struct TypeInstanceInterface {
			tsh::AbstractType type;
		};

		/**
		 * @brief The interface of the type.
		 * The behavior depends on the type, but in general it
		 * represents what `given-type.abc` would do.
		 */
		struct TypeMetaInterface {
			tsh::AbstractType type;
		};

		/**
		 * @brief A custom interface — anyone can create their own interface.
		 */
		struct CustomInterface {
			Box<CustomInterfaceABC> custom;
		};

		using VariantT = std::variant<
			ScopeInterface,
			ScopeWithParentsInterface,
			ModuleInterface,
			NamespaceInterface,
			ImportInterface,
			UsingInterface,
			TypeInstanceInterface,
			TypeMetaInterface,
			CustomInterface>;

		VariantT data;

		explicit HInterface(VariantT data): data(std::move(data)) {}

	public:
		HInterface()                  = delete;
		HInterface(const HInterface&) = delete;
		HInterface(HInterface&&)      = default;

		/**
		 * Performs a lookup in a given interface.
		 */
		CRef<query::QResult<LookupResult>> lookup(
			query::Context& ctx, base::StrID name, AdditionalLookupParameters = {}
		) const;

		/**
		 * A lookup function that performs a most common lookup operation,
		 * hiding a lot of boilerplate associated with it. It performs the following steps:
		 * 1. It looks-ups the interface.
		 * 2. It reports error if more than one symbol is found.
		 * 3. Return the symbol list. Aliases are already resolved by the lookup itself.
		 *
		 * It some error occurs, it will report it in @p error_position.
		 *
		 * @note This function is intended to be used as a quick placeholder
		 * that we might one day change to custom code for better compilation errors or logic.
		 */
		query::QResult<SymbolList> lookupExpectUnique(
			dia::StablePosition error_position,
			query::Context&     ctx,
			base::StrID         name,
			AdditionalLookupParameters = {}
		) const;

		/**
		 * @brief The interface of a symbol, i.e. what `sym.something` looks into.
		 *
		 * Namespace-like symbols (modules, namespaces, usings, imports) look into their own
		 * contents, a class looks into its statics (`MyClass.CONST`) and a symbol that holds a
		 * value looks into the members of its type (`my_var.field`).
		 *
		 * @note Fails, with a logged error, for symbol kinds that have no interface.
		 */
		static HInterface ofSymbol(query::Context& ctx, SymID symbol);

		static HInterface ofScope(const ScopeID scope) {
			return HInterface{ ScopeInterface{ scope } };
		}

		static HInterface ofScopeWithParents(const ScopeID scope) {
			return HInterface{ ScopeWithParentsInterface{ scope } };
		}

		static HInterface ofModule(SymID symbol) { return HInterface{ ModuleInterface{ symbol } }; }

		static HInterface ofNamespace(SymID symbol) {
			return HInterface{ NamespaceInterface{ symbol } };
		}

		static HInterface ofImport(SymID symbol) { return HInterface{ ImportInterface{ symbol } }; }

		static HInterface ofUsing(SymID symbol) { return HInterface{ UsingInterface{ symbol } }; }

		static HInterface ofTypeInstance(const tsh::AbstractType type) {
			return HInterface{ TypeInstanceInterface{ type } };
		}

		static HInterface ofTypeMeta(const tsh::AbstractType type) {
			return HInterface{ TypeMetaInterface{ type } };
		}

		static HInterface ofCustom(Box<CustomInterfaceABC> custom) {
			return HInterface{ CustomInterface{ std::move(custom) } };
		}
	};
}
