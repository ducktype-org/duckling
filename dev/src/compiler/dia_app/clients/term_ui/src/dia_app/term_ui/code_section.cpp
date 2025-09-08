#include "code_section.hpp"

namespace term_ui {
	CodeSection::Location::Location(const std::string& file, u32 line, u32 col):
		  file(file),
		  line(line),
		  col(col) {}

	CodeSection::Location::Location(const view::CodeMetadata& metadata):
		  file(metadata.filename()),
		  line(metadata.line()),
		  col(metadata.column()) {}

	void CodeSection::Location::print(u32 tab_space, std::ostream& out) const {
		out << std::string(tab_space, ' ');
		out << "> " << file << ':' << line << ':' << col << '\n';
	}

	CodeSection::CodeSection(
		Location location, std::vector<CodeLine> lines, base::HashMap<u32, PointerMessage> pointers
	):
		  location(location),
		  lines(lines),
		  pointers(pointers),
		  tab_space(0) {
		computeLastOfAndTabSpace();
	}

	CodeSection::CodeSection(const view::CodeSection& section):
		  location(section.metadata()),
		  tab_space(0) {
		// Extract lines.
		for (u32 i = 0; i < section.lines_size(); ++i) lines.emplace_back(section.lines(i));
		// Extract pointers.
		for (u32 i = 0; i < section.hl_messages_size(); ++i) {
			auto& ptr = section.hl_messages(i);
			pointers.put(ptr.tag(), PointerMessage(ptr));
		}
		// Perform pre-processing.
		computeLastOfAndTabSpace();
	}

	void CodeSection::print(std::ostream& out) const {
		location.print(tab_space, out);
		print_line_start(tab_space, out);
		out << std::endl;
		for (u32 l = 0; l < lines.size(); ++l) {
			auto lowered = lines[l].print(tab_space, pointers, out);

			// Handle buffered pointer messages.
			while (!lowered.empty()) {
				print_line_start(tab_space, out);

				auto this_lowered = std::move(lowered);
				lowered           = std::vector<Highlight>();

				std::sort(this_lowered.begin(), this_lowered.end());
				Line line;

				for (auto& highlight: this_lowered) {
					u32 beg = highlight.beg;
					switch (underline(line, highlight, l)) {
					case HighlightResult::LowerHighlight: {
						// The highlight did not fit.
						lowered.push_back(highlight);
						break;
					}
					case HighlightResult::LowerMessageMedium: {
						// The message did not fit.
						lowered.push_back(highlight.withStage(LoweringStage::Medium));
						break;
					}
					case HighlightResult::LowerMessageLast: {
						// Lowered message's lowering char has been
						// displayed.
						lowered.push_back(highlight.withStage(LoweringStage::Last));
						break;
					}
					case HighlightResult::Success: {
						// The message has been displayed.
						break;
					}
					}
				}
				line.print(out);
			}
		}
	}

	void CodeSection::computeLastOfAndTabSpace() {
		// For each group find the last of its pieces.
		// That's where the message will appear.
		for (u32 l = 0; l < lines.size(); ++l) {
			auto& line = lines[l];

			for (u32 i = 0; i < line.size(); ++i) {
				auto& piece = line[i];
				if (!piece.getGroups().empty())
					for (u32 group: piece.getGroups()) last_of_group.put(group, { l, i });
			}
			tab_space = std::max(tab_space, line.minTabSpace());
		}
	}

	CodeSection::HighlightResult CodeSection::fitLoweredMessage(
		Line& str, u32 beg, const PointerMessage& msg
	) const {
		if (!str.tryInsert(beg, msg.intoLinePiece(PointerStage::Message))) {
			// The message does not fit.
			str.tryInsert(beg, msg.intoLinePiece(PointerStage::Lowering));
			return HighlightResult::LowerMessageLast;
		}
		// The message fits.
		return HighlightResult::Success;
	}

	CodeSection::HighlightResult CodeSection::underline(Line& str, Highlight highlight, u32 line_no)
		const {
		auto [priority, beg, end, group, idx, lowering] = highlight;
		u32   len                                       = end - beg;
		auto& msg                                       = pointers.at(group);

		// Handle lowering stages first.
		if (lowering == LoweringStage::Medium) {
			// The message was lowered but no lowering char has yet been
			// placed (it is required).

			if (str.tryInsert(beg, msg.intoLinePiece(PointerStage::Lowering))) {
				// A lowering char has been placed, now only need to fit
				// the message.
				return HighlightResult::LowerMessageLast;
			} else {
				// A lowering char must yet be displayed.
				return HighlightResult::LowerMessageMedium;
			}
		}
		if (lowering == LoweringStage::Last) {
			// Fit the message (or the next lowering if the message does not
			// fit).
			return fitLoweredMessage(str, beg, msg);
		}
		// There has been no lowering yet.

		// Check if the highlight can be placed in this line.
		if (!str.isEmptyOn(beg, len)) {
			// It cannot.
			return HighlightResult::LowerHighlight;
		}

		if (std::make_pair(line_no, idx) != last_of_group.at(group)) {
			// No message attached, just underline (the message is to be
			// displayed after some further code fragment).
			str.tryInsert(beg, msg.intoLinePiece(PointerStage::Highlight, len));
			return HighlightResult::Success;
		}

		// Check if the message can be placed in this line.
		// ............... message ......
		//                ^^^^^^^^^ these spots need to be free
		//                ^ end
		//                         ^ end + msg.text.size() + 2
		if (!str.isEmptyOn(end, msg.getText().size() + 2)) {
			// It cannot.
			// We already know we can underline, do so with lowering.
			str.tryInsert(beg, msg.intoLinePiece(PointerStage::HighlightWithLowering, len));
			return HighlightResult::LowerMessageMedium;
		}

		// Both the highlight and the message fit.
		str.tryInsert(beg, msg.intoLinePiece(PointerStage::Highlight, len));
		str.tryInsert(end + 1, msg.intoLinePiece(PointerStage::Message));
		return HighlightResult::Success;
	}
}
