#pragma once

#include "lookup_result.hpp"

#include <helios/helios_result.hpp>
#include <typesystem/higher/abstract_type.hpp>

#include <set>

namespace compiler::helios {

	/**
	 * @brief A record which describes a named argument provided to a function call.
	 *
	 * A named argument is described with its name and type.
	 */
	struct NamedArgument final {
		base::StrID       name;
		tsh::AbstractType type;
	};

	/**
	 * @brief A resolution result which means that no match was found.
	 */
	struct NoMatch final {
		/**
		 * @brief Elements with the requested name.
		 */
		std::set<SymID> non_matches;
	};

	/**
	 * @brief A resolution result which means that a single match was found.
	 */
	struct SingleMatch final {
		/**
		 * @brief The best match.
		 */
		 SymID best_match;

		/**
		 * @brief Other members of the same name which matched less accurately.
		 */
		std::set<SymID> alternative_matches;

		/**
		 * @brief Elements with the requested name which did not match.
		 */
		std::set<SymID> non_matches;
	};

	/**
	 * @brief A resolution result which means that member resolution is ambiguous.
	 *
	 * @note An exact match is still possible when the resolution is ambiguous. Consider
	 * the overloaded function `foo` with signatures `foo(a : i32, b : bool = true)` and
	 * `foo(a : i32, c : char = 'a')`. A call of `foo(2)` matches both signatures perfectly,
	 * but remains ambiguous.
	 */
	struct AmbiguousMatch final {
		/**
		 * @brief The conflicting matches.
		 */
		std::set<SymID> conflicting_matches;

		/**
		 * @brief Other members of the same name which matched less accurately.
		 */
		std::set<SymID> alternative_matches;

		/**
		 * @brief Elements with the requested name which did not match.
		 */
		std::set<SymID> non_matches;
	};

	using ResolutionResult = std::variant<NoMatch, SingleMatch, AmbiguousMatch>;

	/**
	 * @TODO: What this should take as a parameter and what it should return.
	 * @brief Overload resolution for a function call.
	 */
	ResolutionResult overloadResolution(
		CRef<LookupResult> symbols,
		const std::vector<tsh::AbstractType>& positional_arg_types,
		const std::set<NamedArgument>&   named_args,
		query::Context&                  ctx
	);

};
