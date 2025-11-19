#include "highlight.hpp"

namespace term_ui {
	Highlight::Highlight(u32 priority, u32 beg, u32 end, u32 group, u32 idx, LoweringStage stage):
		  priority(priority),
		  beg(beg),
		  end(end),
		  group(group),
		  idx(idx),
		  lowering(stage) {}

	bool Highlight::operator<(const Highlight& other) const {
		// Highlight state and position are more important than
		// priority in the static UI.
		if (end == other.end) {
			if (lowering == other.lowering) return priority < other.priority;
			return lowering < other.lowering;
		}
		return end > other.end;
	}

	Highlight Highlight::withStage(LoweringStage stage) const {
		switch (stage) {
		case LoweringStage::None: {
			return *this;
		}
		case LoweringStage::Medium: {
			return Highlight(priority, beg, beg + 1, group, idx, stage);
		}
		case LoweringStage::Last: {
			return Highlight(priority, beg, beg + 1, group, idx, stage);
		}
		}
		return *this;
	}

	PointerMessage::PointerMessage(std::string text, StyleType type, u32 priority):
		  text(text),
		  type(type),
		  priority(priority) {}

	PointerMessage::PointerMessage(const view::HlMessage& message):
		  text(TextPieces(message.message()).to_string()),
		  type(style_type_of(message.type())),
		  priority(message.priority()) {}

	const std::string& PointerMessage::getText() const { return text; }

	u32 PointerMessage::getPriority() const { return priority; }

	LinePiece PointerMessage::intoLinePiece(PointerStage stage, int count) const {
		std::string str;
		Style       style = get_style(type);
		switch (stage) {
		case PointerStage::Highlight: {
			CORE_ASSERT(count > 0, "Cannot use a non-positive highlight width.");
			str = std::string(count, style.underline_char);
			break;
		}
		case PointerStage::HighlightWithLowering: {
			CORE_ASSERT(count > 0, "Cannot use a non-positive highlight width.");
			str = std::string(1, style.lowering_attach_char)
			    + std::string(count - 1, style.underline_char);
			break;
		}
		case PointerStage::Lowering: {
			str = std::string(1, style.lowering_char);
			break;
		}
		case PointerStage::Message: {
			str = text;
			break;
		}
		}
		return LinePiece(str, type);
	}
}
