#include "code_line.hpp"

namespace term_ui {

	u64 minTabSpace(const dia::term_ui_view::CodeLine& line) {
		u64 res = 1;
		if (line.line_no.has_value()) {
			u64 number = line.line_no.value();
			res        = std::to_string(number).size() + 1;
		}
		return res;
	}

	std::vector<Highlight> printAndCalculateHighlights(
		const dia::term_ui_view::CodeLine&                           line,
		u64                                                          tab_space,
		const base::HashMap<u64, dia::term_ui_view::PointerMessage>& ctx,
		std::ostream&                                                out
	) {
		if_opt_some(line.line_no, number) { printLineStart(tab_space, number, out); }
		if_opt_none(line.line_no) { printLineStart(tab_space, out); }

		// This is the relative column from the start of the code line.
		u64 col = 0;

		std::vector<Highlight>     lowered;
		std::vector<std::set<u64>> visited_pointer_messages(line.pieces.size(), std::set<u64>());


		for (u64 i = 0; i < line.pieces.size(); ++i) {
			const auto& piece = line.pieces[i];

			if (piece.pointer_ids.empty()) {
				// Print the code piece.
				out << piece.text;
				col += piece.text.size();
			} else {
				// Buffer all pointer messages.
				for (auto pointer_message_id: piece.pointer_ids) {
					if (visited_pointer_messages[i].contains(pointer_message_id)) {
						// This group on this piece has already been handled.
						continue;
					}

					// Find all contigous pieces in this line and merge them.
					u64 last_idx = i;
					u64 last_col = col;
					while (last_idx < line.pieces.size()
					       && line.pieces[last_idx].pointer_ids.contains(pointer_message_id)) {
						visited_pointer_messages[last_idx].insert(pointer_message_id);
						last_col += line.pieces[last_idx].text.size();
						++last_idx;
					}
					lowered.emplace_back(
						ctx.at(pointer_message_id).priority,
						col,
						last_col,
						pointer_message_id,
						last_idx - 1,
						LoweringStage::None
					);
				}

				// Print the code piece.
				out << piece.text;
				col += piece.text.size();
			}
		}
		out << '\n';
		return lowered;
	}
}
