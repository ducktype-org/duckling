#pragma once
#include "diagnostic.hpp"
#include <vector>

/**
 * @brief The number of columns used to display an info group (diagnostic)
 * separator (consisting of symbols `-`).
 *
 */
#define TERM_UI_SEPARATOR_WIDTH 80

namespace term_ui {
	/**
	 * @brief Print the view to output stream.
	 *
	 * @param diags The list of diagnostics to print.
	 * @param out The output stream.
	 * @param use_color Whether to use coloring in the displayed view.
	 */
	void print(const std::vector<dia_app::term_ui_view::Diagnostic>& diags, std::ostream& out, bool use_color);
}
