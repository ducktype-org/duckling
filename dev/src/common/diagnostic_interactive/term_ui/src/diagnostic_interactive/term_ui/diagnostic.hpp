#pragma once
#include <diagnostic_interactive/core/term_ui_view.hpp>
#include <iostream>

namespace term_ui {
	/**
	 * @brief Print this diagnostic to output stream.
	 *
	 * @param diag The diagnostic to print.
	 * @param out The output stream.
	 */
	void print(const dia_app::term_ui_view::Diagnostic& diag, std::ostream& out);
}
