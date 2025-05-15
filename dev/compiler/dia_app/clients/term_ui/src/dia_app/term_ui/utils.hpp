#pragma once

#include <iostream>
#include <vector>
#include <map>
#include "styles.hpp"

namespace term_ui {
    #ifndef NDEBUG
    #   define ASSERT(condition, message) \
        do { \
            if (! (condition)) { \
                std::cerr << "Assertion `" #condition "` failed in " << __FILE__ \
                        << " line " << __LINE__ << ": " << message << std::endl; \
                std::terminate(); \
            } \
        } while (false)
    #else
    #   define ASSERT(condition, message) do { } while (false)
    #endif

    #define ASSUME(expr, msg) ASSERT(expr, msg)

    template <typename T>
    using vec = std::vector<T>;

    inline void print_line_start(uint tab_space, uint line_no) {
        std::string line_no_str = std::to_string(line_no);
        std::string line_start =
            // Print line number.
            line_no_str +
            // Fill the whitespace before the line bar.
            std::string(tab_space - std::to_string(line_no).size(), ' ') +
            // Print the line bar.
            "| ";
    
        LINE_START_STYLE.print_with(line_start);
    }

    inline void print_line_start(uint tab_space) {
        // Print tab space and line bar.
        LINE_START_STYLE.print_with(std::string(tab_space, ' ') + "| ");
    }

    /// @brief Piece of formatted text to be placed in a line.
    struct LinePiece {
        std::string text;
        StyleType type;

        LinePiece(std::string text, StyleType type) :
            text(text), type(type) {}

        void print() const {
            get_style(type).print_with(text);
        }
    };

    /// @brief Line of formatted text. Insert-only, does not allow inserting
    /// overlapping fragments.
    struct Line {
        std::map<uint, LinePiece> pieces;

        // Check if the line is empty on interval [beg, beg + len - 1].
        bool is_empty_on(uint beg, uint len) const {
            uint end = beg + len;
            for (auto &[col, piece] : pieces) {
                uint col_end = col + piece.text.size();
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
        bool try_insert(uint beg, const LinePiece &piece) {
            if (!is_empty_on(beg, piece.text.size())) {
                return false;
            }
            pieces.insert({beg, piece});
            return true;
        }

        // Print the entire line, inserting whitespace in gaps between line pieces.
        void print() const {
            uint col = 0;

            for (auto &[beg, piece] : pieces) {
                // Fill the gap with empty characters.
                if (beg > col) {
                    auto fill = std::string(beg - col, ' ');
                    std::cerr << fill;
                }
                // Print the piece.
                piece.print();
                col = beg + piece.text.size();
            }

            std::cerr << std::endl;
        }
    };
}