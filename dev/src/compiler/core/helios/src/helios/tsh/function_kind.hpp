// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <cstdint>

namespace compiler::tsh {
	/**
	 * @brief Enum representing the kind of a function type. See FunctionAbstractType for details.
	 */
	enum class FunctionKind : uint8_t {
		/**
		 * @brief A pointer to the code of a function, written `funptr(A) -> R`.
		 */
		Raw,

		/**
		 * @brief A callable that may carry an environment, written `(A) -> R`.
		 */
		Closure,
	};
}
