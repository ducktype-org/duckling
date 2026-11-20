// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once
#include <diagnostic/core/term_ui_view.hpp>

#include <iostream>

/**
 * @brief The number of columns used to display an info group (diagnostic)
 * separator (consisting of symbols `-`).
 *
 */
constexpr std::size_t TERM_UI_SEPARATOR_WIDTH = 80;

namespace term_ui {
	/**
	 * @brief Print the term ui view to output stream.
	 *
	 * @param diags The list of diagnostics to print.
	 * @param out The output stream.
	 * @param use_color Whether to use coloring in the displayed view.
	 */
	void print(
		const std::vector<dia::term_ui_view::Diagnostic>& diags, std::ostream& out, bool use_color
	);

	/**
	 * @brief Print this diagnostic to output stream.
	 *
	 * @param diag The diagnostic to print.
	 * @param out The output stream.
	 */
	void print(const dia::term_ui_view::Diagnostic& diag, std::ostream& out);

	/**
	 * @brief Print hte message to output stream.
	 */
	void print(const dia::term_ui_view::Message& msg, std::ostream& out);
}
