#pragma once

#include "lookup_result.hpp"

#include <helios/helios_result.hpp>
#include <typesystem/higher/abstract_type.hpp>

#include <base/box.hpp>

#include <variant>

namespace dia {
	// Forward declaration of SourcePosition
	class SourcePosition;
}

namespace compiler::helios {

	struct AdditionalLookupParameters final {
		bool with_wildcards = true;
		// @TODO: public/private/protected?
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
		virtual CRef<LookupResult> lookup(
			query::Context& ctx, base::StrID name, AdditionalLookupParameters params
		) = 0;
	};

	/**
	 * Interface is an abstraction over the lookup process.
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
		 * @brief The interface of the symbol.
		 * The behavior depends on the symbol type, but usually it
		 * represents what `symbol.abc` would do.
		 */
		struct SymbolInterface {
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
		 * @brief A custom interface -- anyone can create their own interface.
		 */
		struct CustomInterface {
			Box<CustomInterfaceABC> custom;
		};

		using VariantT = std::variant<
			ScopeInterface,
			ScopeWithParentsInterface,
			SymbolInterface,
			TypeInstanceInterface,
			TypeMetaInterface,
			CustomInterface>;

		VariantT data;

		HInterface(VariantT data): data(std::move(data)) {}

	public:
		HInterface()                  = delete;
		HInterface(const HInterface&) = delete;
		HInterface(HInterface&&)      = default;

		/**
		 * Performs a lookup in a given interface.
		 */
		CRef<LookupResult> lookup(
			query::Context& ctx, base::StrID name, AdditionalLookupParameters = {}
		);

		/**
		 * A lookup function that performs a most common lookup operation,
		 * hiding a lot of boilerplate associated with it. It performs the following steps:
		 * 1. It looks-ups the interface.
		 * 2. It reports error if more than one symbol is found.
		 * 3. It performs deliasing if needed.
		 * 4. Return dealiased symbol list.
		 *
		 * It some error occurs, it will report it in @p error_position.
		 *
		 * @note This function is intended to be used as a quick placeholder
		 * that we might one day change to custom code for better compilation errors or logic.
		 */
		errors::HResult<SymbolList, errors::Failed> lookupExpectUnique(
			dia::SourcePosition error_position,
			query::Context&     ctx,
			base::StrID         name,
			AdditionalLookupParameters = {}
		);

		static HInterface ofScope(ScopeID scope) { return HInterface{ ScopeInterface{ scope } }; }

		static HInterface ofScopeWithParents(ScopeID scope) {
			return HInterface{ ScopeWithParentsInterface{ scope } };
		}

		static HInterface ofSymbol(SymID symbol) { return HInterface{ SymbolInterface{ symbol } }; }

		static HInterface ofTypeInstance(tsh::AbstractType type) {
			return HInterface{ TypeInstanceInterface{ type } };
		}

		static HInterface ofTypeMeta(tsh::AbstractType type) {
			return HInterface{ TypeMetaInterface{ type } };
		}

		static HInterface ofCustom(Box<CustomInterfaceABC> custom) {
			return HInterface{ CustomInterface{ std::move(custom) } };
		}
	};
}
