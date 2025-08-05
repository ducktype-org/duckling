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

        Highlight(uint priority, uint beg, uint end, uint group, uint idx, LoweringStage stage);

        bool operator<(const Highlight &other) const;

        Highlight withStage(LoweringStage stage) const;
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
        PointerMessage(std::string text, StyleType type, uint priority = 0);

        PointerMessage(const view::HlMessage &message);
        
        const std::string &getText() const;

        uint getPriority() const;

        LinePiece intoLinePiece(PointerStage stage, int count = -1) const;
    };
}