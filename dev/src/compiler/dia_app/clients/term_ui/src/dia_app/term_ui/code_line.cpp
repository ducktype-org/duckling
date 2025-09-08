#include "code_line.hpp"

namespace term_ui {
	CodeLine::CodeLine(u32 line_no, std::vector<CodePiece> pieces):
		  line_no(line_no),
		  pieces(pieces) {}

	CodeLine::CodeLine(const view::CodeLine& line): pieces(CodePieces(line.content()).getPieces()) {
		if (line.has_line_number()) line_no = line.line_number();
	}

	CodePiece& CodeLine::operator[](const u32 i) { return pieces[i]; }

	u32 CodeLine::size() const { return pieces.size(); }

	u32 CodeLine::minTabSpace() const {
		if_opt_some(line_no, number) { return std::to_string(number).size() + 1; }
		else {
			// Always at least one whitespace is displayed.
			return 1;
		}
	}

	std::vector<Highlight> CodeLine::print(
		u32 tab_space, const base::HashMap<u32, PointerMessage>& ctx, std::ostream& out
	) const {
		if_opt_some(line_no, number) { print_line_start(tab_space, number, out); }
		else { print_line_start(tab_space, out); }

		// This is the relative column from the start of the code line.
		u32 col = 0;

		std::vector<Highlight>     lowered;
		std::vector<std::set<u32>> visited_groups(pieces.size(), std::set<u32>());

		for (u32 i = 0; i < pieces.size(); ++i) {
			auto& piece = pieces[i];

			if (piece.getGroups().empty()) {
				// Print the code piece.
				out << piece.getText();
				col += piece.getText().size();
			} else {
				// Buffer all pointer messages.
				for (auto group: piece.getGroups()) {
					if (visited_groups[i].contains(group)) {
						// This group on this piece has already been handled.
						continue;
					}

					// Find all contigous pieces in this line and merge them.
					u32 last_idx = i;
					u32 last_col = col;
					while (last_idx < pieces.size() && pieces[last_idx].getGroups().contains(group)
					) {
						visited_groups[last_idx].insert(group);
						last_col += pieces[last_idx].getText().size();
						++last_idx;
					}
					lowered.emplace_back(
						ctx.at(group).getPriority(),
						col,
						last_col,
						group,
						last_idx - 1,
						LoweringStage::None
					);
				}

				// Print the code piece.
				out << piece.getText();
				col += piece.getText().size();
			}
		}
		out << '\n';
		return lowered;
	}
}
