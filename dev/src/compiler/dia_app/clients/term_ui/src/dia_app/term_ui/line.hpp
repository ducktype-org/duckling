#pragma once
#include <vector>
#include <iostream>

#include "styles.hpp"

namespace term_ui {

    /// @brief Piece of formatted text to be placed in a line.
    class LinePiece {
        std::string text;
        StyleType type;
    public:
        LinePiece(std::string text, StyleType type);
        
        const std::string getText() const;

        void print(std::ostream& out) const;
    };

    /// @brief Line of formatted text. Insert-only, does not allow inserting
    /// overlapping fragments.
    class Line {
        std::map<uint, LinePiece> pieces;
    public:
        // Check if the line is empty on interval [beg, beg + len - 1].
        bool isEmptyOn(uint beg, uint len) const;
        
        // Try to insert a new line piece into this line starting on column beg.
        // Return true on success and false on failure.
        bool tryInsert(uint beg, const LinePiece &piece);

        // Print the entire line, inserting whitespace in gaps between line pieces.
        void print(std::ostream& out) const;
    };
}