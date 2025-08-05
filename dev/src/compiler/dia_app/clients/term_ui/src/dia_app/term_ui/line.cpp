#include "line.hpp"

namespace term_ui {
    LinePiece::LinePiece(std::string text, StyleType type) :
        text(text), type(type) {}
    
    const std::string LinePiece::getText() const {
        return text;
    }

    void LinePiece::print(std::ostream& out) const {
        get_style(type).printWith(text, out);
    }


    bool Line::isEmptyOn(uint beg, uint len) const {
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

    bool Line::tryInsert(uint beg, const LinePiece &piece) {
        if (!isEmptyOn(beg, piece.getText().size())) {
            return false;
        }
        pieces.insert({beg, piece});
        return true;
    }

    void Line::print(std::ostream& out) const {
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
}