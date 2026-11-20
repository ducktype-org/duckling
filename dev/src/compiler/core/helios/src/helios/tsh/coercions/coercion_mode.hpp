// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file coercion_mode.hpp
 * @brief Which coercions are allowed for the location a value is handed to.
 */

#pragma once

#include <cstdint>

namespace compiler::tsh::coercions {
	/**
	 * @brief Which coercions are allowed for the location the value is handed to.
	 */
	enum class CoercionMode : uint8_t {
		/// Only the regular implicit coercions.
		Default,
		/// A non-`ref` value may be implicitly taken by `ref` (a method's `self`).
		ImplicitRefOf,
	};
}
