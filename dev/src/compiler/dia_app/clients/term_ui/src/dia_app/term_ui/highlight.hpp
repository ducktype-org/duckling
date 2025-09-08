#pragma once
#include "component_pieces.hpp"
#include "line.hpp"
#include "styles.hpp"

#include <proto/view.pb.h>

namespace term_ui {
	/**
	 * @brief The stage of lowering a highlight with a specified ordering.
	 *
	 * Note: numbers refer the to order of evalutation of highlights
	 * when they end on the same spot.
	 *
	 * Because Last < Medium, we get:
	 * ```
	 *      |
	 *      message_1
	 *      |
	 *      message_2
	 * ```
	 * instead of:
	 * ```
	 *      |
	 *      |
	 *      message_1
	 *      message_2
	 * ```
	 */
	enum class LoweringStage {
		None   = 0,  // Has not printed the highlight yet.
		Medium = 2,  // At least one lowering_char must yet be displayed.
		Last   = 1   // Here comes the message.
	};

	/**
	 * @brief Highlight information for one code fragment and one highlight.
	 *
	 */
	struct Highlight {
		// The priority of this highlight (used for determining the display
		// order of different highlights).
		u32 priority;
		// The bounds of the highlighted code.
		u32 beg, end;
		// The group ID this highlight refers to.
		u32 group;
		// The index of the last highlighted code piece (used for determining
		// the display order of different highlights).
		u32 idx;
		// The lowering stage this highlight is in.
		LoweringStage lowering;

		// Construct a new highlight from field values.
		Highlight(u32 priority, u32 beg, u32 end, u32 group, u32 idx, LoweringStage stage);

		/**
		 * @brief The ordering of highlights.
		 *
		 * In order to save up display space, highlights are mainly sorted
		 * based on their `end` field. The next deciding factor is a lowering
		 * stage, and, at last, the priority breaks ties.
		 */
		bool operator<(const Highlight& other) const;

		/**
		 * @brief Get a copy of this highlight with a modified lowering stage.
		 *
		 * Note that the resulting highlight may also have other fields
		 * (namely, the `end`) modified due to the lowering stage change.
		 *
		 * @param stage The target stage.
		 * @return Highlight
		 */
		Highlight withStage(LoweringStage stage) const;
	};

	/**
	 * @brief The stage a pointer (highlight) message is in.
	 *
	 */
	enum class PointerStage {
		// Only highlight chars should be displayed now.
		Highlight,
		// Only highlight chars should be displayed now except for the first
		// one, which should be an lowering attach char.
		HighlightWithLowering,
		// Only a lowering char should be displayed now.
		Lowering,
		// Now the highlight message text should be displayed.
		Message
	};

	/**
	 * @brief A pointer (highlight) message constructed from view manager data.
	 *
	 */
	class PointerMessage {
		// The highlight message text.
		std::string text;
		// The style type this message and highlight should be displayed with.
		StyleType type;
		// The message priority, as read from the view manager data.
		u32 priority;

	public:
		// Construct a new message from a view manager highlight message.
		PointerMessage(const view::HlMessage& message);

		// Get the message text.
		const std::string& getText() const;

		// Get the priority.
		u32 getPriority() const;

		/**
		 * @brief Generate a printable line piece from this message, provided
		 * it is in a given stage.
		 *
		 * @param stage The stage this message is recorder at.
		 * @param count The width of the highlight (only required for the
		 * `Highlight` and `HighlightWithLowering` stages).
		 * @return LinePiece
		 */
		LinePiece intoLinePiece(PointerStage stage, int count = -1) const;

		// Construct a new message from field values. For testing only.
		PointerMessage(std::string text, StyleType type, u32 priority = 0);
	};
}
