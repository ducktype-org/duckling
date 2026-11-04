// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

namespace term_ui {
	/**
	 * @brief A global flag for using colors when displaying the view.
	 *
	 * We decided for such a simple mechanism, since displaying the diagnostic
	 * view should be an atomic operation anyways and having a global flag
	 * simplifies the code (there is no need to pass that flag down through
	 * a multitude of methods).
	 */
	extern bool use_color;

	/**
	 * @brief Configure whether to use colors in the terminal UI.
	 *
	 * Also configures the underlying rang library to always output
	 * escape codes by forcefully emitting ANSI instead of relying on
	 * stdout TTY detection.
	 */
	void configureColoring(bool use);
}
