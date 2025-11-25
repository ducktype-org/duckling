#pragma once
#include "highlight.hpp"
#include "styles.hpp"

#include <diagnostic_interactive/core/term_ui_view.hpp>

#include <base/maps.hpp>
#include <base/optional.hpp>

namespace term_ui {
	/* Minimal amount of whitespace needed before the line bar `|`. */
	u64 minTabSpace(const dia_app::term_ui_view::CodeLine& line);

	/**
	 * @brief Print the code line to the output stream.
	 *
	 * Returns a list of highlights to be displayed in the following lines.
	 *
	 * @param line The code line to print.
	 * @param tab_space The column where the code should begin.
	 * @param ctx A lookup map for highlight message contents.
	 * @param out The output stream.
	 * @return std::vector<Highlight>
	 */
	std::vector<Highlight> print(
		const dia_app::term_ui_view::CodeLine&            line,
		u64                                               tab_space,
		const base::HashMap<u64, dia_app::term_ui_view::PointerMessage>& ctx,
		std::ostream&                                     out
	);
}
