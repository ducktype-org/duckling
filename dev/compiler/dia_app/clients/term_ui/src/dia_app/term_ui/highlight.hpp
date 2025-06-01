#pragma once
#include <proto/view.pb.h>
#include "styles.hpp"
#include "component_pieces.hpp"
#include "line.hpp"

namespace term_ui {
    /*
        Note: numbers refer the to order of evalutation of highlights
            when they end on the same spot.

        Because Last < Medium, we get:
            |
            message_1
            |
            message_2,
        
        instead of:
            |
            |
            message_1
            message_2.
    */
    enum class LoweringStage {
        None = 0, // Has not printed the highlight yet.
        Medium = 2, // At least one lowering_char must yet be displayed.
        Last = 1 // Here comes the message.
    };

    struct Highlight {
        uint priority;
        uint beg, end;
        uint group;
        uint idx; // Index of the last highlighted code piece.
        LoweringStage lowering;

        Highlight(uint priority, uint beg, uint end, uint group, uint idx, LoweringStage stage) :
            priority(priority), beg(beg), end(end), group(group), idx(idx), lowering(stage) {}

        bool operator<(const Highlight &other) const {
            // Highlight state and position are more important than
            // priority in the static UI.
            if (end == other.end) {
                if (lowering == other.lowering) {
                    return priority < other.priority;
                }
                return lowering < other.lowering;
            }
            return end > other.end;
        }

        Highlight withStage(LoweringStage stage) const {
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
    };

    enum class PointerStage {
        Highlight,
        HighlightWithLowering,
        Lowering,
        Message
    };

    class PointerMessage {
        std::string text;
        StyleType type;
        uint priority;
    public:
        // Constructor for testing purposes only.
        PointerMessage(std::string text, StyleType type, uint priority = 0) :
            text(text), type(type), priority(priority) {}

        PointerMessage(const view::HlMessage &message) :
            text(TextPieces(message.message()).to_string()), type(style_type_of(message.type())), priority(message.priority()) {}
        
        const std::string &getText() const {
            return text;
        }

        uint getPriority() const {
            return priority;
        }

        LinePiece intoLinePiece(PointerStage stage, int count = -1) const {
            std::string str;
            Style style = get_style(type);
            switch (stage) {
                case PointerStage::Highlight: {
                    assert(count > 0);
                    str = std::string(count, style.underline_char);
                    break;
                }
                case PointerStage::HighlightWithLowering: {
                    assert(count > 0);
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
    };
}