// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
