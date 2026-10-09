// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <diagnostic/core/term_ui_view.hpp>

namespace term_ui {

	/**
	 * @brief Print the code section to the output stream.
	 *
	 * @param section The code section to print.
	 * @param out The output stream.
	 */
	void print(const dia::term_ui_view::CodeSection& section, std::ostream& out);
}
