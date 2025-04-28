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
		bool with_wildcards = true;  // this PR: check it
									 // @TODO: public/private/protected
	};

	/**
	 * @brief A class that can be used to create custom interfaces thrue inheritance.
	 */
	class CustomInterface {
	public:
		virtual ~CustomInterface() = default;

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
		 * @brief The interface of the scope
		 */
		struct ScopeInterface {
			helios::ScopeID scope;
		};

		/**
		 * @brief The interface of the scope and all its parents
		 */
		struct ScopeWithParentsInterface {
			helios::ScopeID scope;
		};

		/**
		 * @brief The interface of the symbol
		 */
		struct SymbolInterface {
			helios::SymID symbol;
		};

		/**
		 * @brief The interface of the type
		 */
		struct TypeInterface {
			tsh::AbstractType type;
		};

		/**
		 * @brief A custom interface -- anyone can create their own interface.
		 */
		struct CucstomInterface {
			Box<CustomInterface> custom;
		};

		using VariantT = std::variant<
			ScopeInterface,
			ScopeWithParentsInterface,
			SymbolInterface,
			TypeInterface,
			CucstomInterface>;

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
		 * A lookup function that performs a typical simple lookup, that is:
		 * 1. It looks-ups the interface
		 * 2. It reports error if more then one symbol is found
		 * 3. It performs deliasing if needed
		 * 4. Return dealiased symbol list
		 *
		 * It some error occures, it will report it in @p error_position.
		 *
		 * @note This function is intended to be used as a quick placeholder
		 * that we migth oneday change to custom code for better compilation errors.
		 */
		errors::HResult<SymbolList, errors::Failed> typicalSimpleLookup(
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

		static HInterface ofType(tsh::AbstractType type) {
			return HInterface{ TypeInterface{ type } };
		}

		static HInterface ofCustom(Box<CustomInterface> custom) {
			return HInterface{ CucstomInterface{ std::move(custom) } };
		}
	};

	// some notes:
	// interface should be responsible for performing lookup only,
	// i.e. generating LookupResult.
	// overload resolution should happen in lookup_result probably.
	// lookup result needs some fine tuning.
	//
	// We maybe need a way to figure out if a given symbol is a method of a proper class


}
