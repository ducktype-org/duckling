#pragma once
#include "component_pieces.hpp"
#include "highlight.hpp"
#include "styles.hpp"

#include <base/maps.hpp>
#include <base/optional.hpp>

namespace term_ui {
	/**
	 * @brief The class responsible for displaying a single code line from
	 * a code section.
	 */
	struct CodeLine {
		// An optional line number. To be displayed before the line.
		base::Optional<u32> line_no;
		// A list of code pieces to be displayed in order.
		std::vector<CodePiece> pieces;

		// Construct a new code line from the data provided by the view manager.
		CodeLine(const view::CodeLine& line);

		// Get the `i`-th code piece.
		CodePiece& operator[](const u32 i);

		// Get the number of code pieces.
		u32 size() const;

		/* Minimal amount of whitespace needed before the line bar `|`. */
		u32 minTabSpace() const;

		/**
		 * @brief Print the code line to the output stream.
		 *
		 * Returns a list of highlights to be displayed in the following lines.
		 *
		 * @param tab_space The column where the code should begin.
		 * @param ctx A lookup map for highlight message contents.
		 * @param out The output stream.
		 * @return std::vector<Highlight>
		 */
		std::vector<Highlight> print(
			u32 tab_space, const base::HashMap<u32, PointerMessage>& ctx, std::ostream& out
		) const;

		// Construct a new code line without a gRPC object. For testing only.
		CodeLine(u32 line_no, std::vector<CodePiece> pieces);
	};
}
