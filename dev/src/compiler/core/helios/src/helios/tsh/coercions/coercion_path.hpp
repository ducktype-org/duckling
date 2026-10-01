/**
 * @file coercion_path.hpp
 * @brief Where in a type a sub-coercion sits.
 *
 * @TODO: #3691 Generalize and expand this as well.
 */

#pragma once

#include <base/types/ints.hpp>

#include <cstdint>

namespace compiler::tsh::coercions {
	/**
	 * @brief The information about "which part of the type above it" this part is.
	 * @see CoercionError doc comment for more info.
	 */
	struct CoercionPath final {
		enum class In : uint8_t {
			/// We're a component of a composite type (i.e. a tuple element).
			Component,
			/// We're one of the variant alternatives.
			Alternative,
			/// We're one parameter of a function type.
			Parameter,
			/// We're a result of a function type.
			Result,
		};

		In in{ In::Component };

		/// Which one we are. Unused by `Result`.
		usize index{ 0 };
	};
}
