// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "code_section.hpp"

#include <base/except/exceptions.hpp>

#include <diagnostic/term_ui/code_line.hpp>
#include <diagnostic/term_ui/highlight.hpp>
#include <diagnostic/term_ui/line.hpp>

#include <algorithm>

namespace term_ui {

	enum class HighlightResult { LowerHighlight, LowerMessageMedium, LowerMessageLast, Success };

	HighlightResult fitLoweredMessage(
		Line& str, u64 beg, const dia::term_ui_view::PointerMessage& msg
	) {
		if (!str.tryInsert(beg, intoLinePiece(msg, PointerStage::Message))) {
			str.tryInsert(beg, intoLinePiece(msg, PointerStage::Lowering));
			return HighlightResult::LowerMessageLast;
		}
		return HighlightResult::Success;
	}

	/**
	 * @brief Renders the highlight into the line and decides the next state of the highlight.
	 *
	 * @param[out] str the place where the part of the highlight will be rendered
	 * @param highlight the current highlight state
	 * @param line_no line number
	 * @param pointers map of pointer messages
	 * @param last_pointer_message_occurence map of last occurence of pointer messages
	 * @return HighlightResult information about the next state (meaning what to render next) of the
	 * highlight
	 */
	HighlightResult renderHighlightAndMoveToNextState(
		Line&                                                        str,
		Highlight                                                    highlight,
		u64                                                          line_no,
		const base::HashMap<u64, dia::term_ui_view::PointerMessage>& pointers,
		const base::HashMap<u64, std::pair<u64, u64>>&               last_pointer_message_occurence
	) {
		auto [priority, beg, end, group, idx, lowering] = highlight;
		u64         len                                 = end - beg;
		const auto& msg                                 = pointers.at(group);

		switch (lowering) {
		case LoweringStage::Medium: {
			if (str.tryInsert(beg, intoLinePiece(msg, PointerStage::Lowering)))
				return HighlightResult::LowerMessageLast;
			else
				return HighlightResult::LowerMessageMedium;
		}
		case LoweringStage::Last: {
			return fitLoweredMessage(str, beg, msg);
		}
		case LoweringStage::None: {
			// The place for the highlight (`^^^^`) is already occupied.
			if (!str.isEmptyOn(beg, len)) return HighlightResult::LowerHighlight;

			// Not the last piece in the group, so just highlight without message.
			// For example, in:
			//   some |code here| and |more code|
			//        ^^^^^^^^^^^     ^^^^^^^^^^
			// Or in future lines
			if (std::make_pair(line_no, idx) != last_pointer_message_occurence.at(group)) {
				str.tryInsert(beg, intoLinePiece(msg, PointerStage::Highlight, len));
				return HighlightResult::Success;
			}

			// Last piece in the group, but the message doesn't fit after highlight.
			if (!str.isEmptyOn(end, msg.text.size() + 2)) {
				str.tryInsert(beg, intoLinePiece(msg, PointerStage::HighlightWithLowering, len));
				return HighlightResult::LowerMessageMedium;
			}

			// Last piece in the group, and the message fits after highlight.
			str.tryInsert(beg, intoLinePiece(msg, PointerStage::Highlight, len));
			str.tryInsert(end + 1, intoLinePiece(msg, PointerStage::Message));
			return HighlightResult::Success;
		}
		}
		CORE_UNREACHABLE();
	}

	void print(const dia::term_ui_view::CodeSection& section, std::ostream& out) {
		// Preprocessing
		base::HashMap<u64, std::pair<u64, u64>> last_pointer_message_positions;
		u64                                     tab_space = 0;

		// for (auto x : section.pointers) {
		// 	std::cout << "Pointer " << x.first << ": " << x.second.text << "\n";
		// }

		for (u64 l = 0; l < section.lines.size(); l++) {
			const auto& line = section.lines[l];
			for (u64 i = 0; i < line.pieces.size(); i++) {
				const auto& piece = line.pieces[i];
				if (!piece.pointer_ids.empty())
					for (u64 group: piece.pointer_ids)
						last_pointer_message_positions.insertOrAssign(group, { l, i });
			}
			tab_space = std::max(tab_space, minTabSpace(line));
		}

		// Print location
		out << std::string(tab_space, ' ');
		std::println(
			out, "> {}:{}:{}", section.location.file, section.location.line, section.location.column
		);

		if (section.lines.empty()) return;
		printLineStart(tab_space, out);
		out << '\n';

		for (u64 l = 0; l < section.lines.size(); ++l) {
			auto lowered
				= printAndCalculateHighlights(section.lines[l], tab_space, section.pointers, out);

			while (!lowered.empty()) {
				printLineStart(tab_space, out);

				auto this_lowered = std::move(lowered);
				lowered           = std::vector<Highlight>();

				std::ranges::sort(this_lowered, [](const Highlight& lhs, const Highlight& rhs) {
					return lhs < rhs;
				});
				Line line;

				for (auto& highlight: this_lowered) {
					auto next_state_result = renderHighlightAndMoveToNextState(
						line, highlight, l, section.pointers, last_pointer_message_positions
					);
					switch (next_state_result) {
					case HighlightResult::LowerHighlight: {
						lowered.push_back(highlight);
						break;
					}
					case HighlightResult::LowerMessageMedium: {
						lowered.push_back(highlight.withStage(LoweringStage::Medium));
						break;
					}
					case HighlightResult::LowerMessageLast: {
						lowered.push_back(highlight.withStage(LoweringStage::Last));
						break;
					}
					case HighlightResult::Success: {
						break;
					}
					}
				}
				line.print(out);
			}
		}
	}
}
