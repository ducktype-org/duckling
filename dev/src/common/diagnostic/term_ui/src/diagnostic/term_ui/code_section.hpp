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
