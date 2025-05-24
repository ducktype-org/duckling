#pragma once
#include <vector>
#include <iostream>

namespace term_ui {

    /// @brief Piece of formatted text to be placed in a line.
    class LinePiece {
        std::string text;
        StyleType type;
    public:
        LinePiece(std::string text, StyleType type) :
            text(text), type(type) {}
        
        const std::string getText() const {
            return text;
        }

        void print(std::ostream& out) const {
            get_style(type).printWith(text, out);
        }
    };

    /// @brief Line of formatted text. Insert-only, does not allow inserting
    /// overlapping fragments.
    class Line {
        std::map<uint, LinePiece> pieces;
    public:
        // Check if the line is empty on interval [beg, beg + len - 1].
        bool isEmptyOn(uint beg, uint len) const {
            uint end = beg + len;
            for (auto &[col, piece] : pieces) {
                uint col_end = col + piece.getText().size();
                if (col < beg && col_end <= beg) {
                    continue;
                }
                if (beg < col && end <= col) {
                    // Pieces are sorted.
                    return true;
                }
                return false;
            }
            return true;
        }

        // Try to insert a new line piece into this line starting on column beg.
        // Return true on success and false on failure.
        bool tryInsert(uint beg, const LinePiece &piece) {
            if (!isEmptyOn(beg, piece.getText().size())) {
                return false;
            }
            pieces.insert({beg, piece});
            return true;
        }

        // Print the entire line, inserting whitespace in gaps between line pieces.
        void print(std::ostream& out) const {
            uint col = 0;

            for (auto &[beg, piece] : pieces) {
                // Fill the gap with empty characters.
                if (beg > col) {
                    auto fill = std::string(beg - col, ' ');
                    out << fill;
                }
                // Print the piece.
                piece.print(out);
                col = beg + piece.getText().size();
            }
            out << '\n';
        }
    };
}