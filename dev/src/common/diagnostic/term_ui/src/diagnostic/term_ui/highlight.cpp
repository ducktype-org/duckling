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
			return { priority, beg, beg + 1, group, idx, stage };
		}
		case LoweringStage::Last: {
			return { priority, beg, beg + 1, group, idx, stage };
		}
		}
		return *this;
	}

	LinePiece intoLinePiece(
		const dia::term_ui_view::PointerMessage& msg, PointerStage stage, base::Optional<u64> count
	) {
		std::string str;
		Style       style = getStyleFromType(msg.type);
		switch (stage) {
		case PointerStage::Highlight: {
			CORE_ASSERT(count > 0, "Cannot use a non-positive highlight width.");
			str = std::string(count.value(), style.underline_char);
			break;
		}
		case PointerStage::HighlightWithLowering: {
			CORE_ASSERT(count > 0, "Cannot use a non-positive highlight width.");

			str = std::string(1, style.lowering_attach_char)
			    + std::string(count.value() - 1, style.underline_char);
			break;
		}
		case PointerStage::Lowering: {
			str = std::string(1, style.lowering_char);
			break;
		}
		case PointerStage::Message: {
			str = msg.text;
			break;
		}
		}
		return { str, msg.type };
	}
}
