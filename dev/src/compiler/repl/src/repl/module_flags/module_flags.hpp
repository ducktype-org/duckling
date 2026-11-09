// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

namespace compiler::repl {
#ifdef USE_REPLXX
	/**
	 * @brief Default completion toggle for the REPL frontend.
	 */
	constexpr bool FRONTEND_DEFAULT_COMPLETIONS_ENABLED = true;
#else
	/**
	 * @brief Default completion toggle for the REPL frontend.
	 */
	constexpr bool FRONTEND_DEFAULT_COMPLETIONS_ENABLED = false;
#endif

	/**
	 * @brief Default bracketed paste toggle for the REPL frontend.
	 * https://en.wikipedia.org/wiki/Bracketed-paste
	 */
	constexpr bool FRONTEND_DEFAULT_BRACKETED_PASTE_ENABLED = true;

	/**
	 * @brief Default decorative output toggle for the REPL frontend.
	 */
	constexpr bool FRONTEND_DEFAULT_DECORATIVE_OUTPUT = true;
}
