#pragma once

namespace compiler::tsh {
	/**
	 * @brief Enum representing mutability used in the type system.
	 */
	enum class Mutability : bool {
		/**
		 * @brief The value is mutable.
		 */
		Mutable,

		/**
		 * @brief The value is immutable.
		 */
		Immutable,
	};

}
