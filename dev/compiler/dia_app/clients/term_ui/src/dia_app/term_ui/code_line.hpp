#pragma once
#include "component_pieces.hpp"
#include "highlight.hpp"
#include "styles.hpp"

namespace term_ui {
    struct CodeLine {
        std::optional<uint> line_no;
        std::vector<CodePiece> pieces;

        CodeLine(uint line_no, std::vector<CodePiece> pieces) : line_no(line_no), pieces(pieces) {}

        CodeLine(const view::CodeLine &line) :
            line_no(line.line_number()), pieces(CodePieces(line.content()).getPieces()) {}

        CodePiece &operator[](const uint i) {
            return pieces[i];
        }

        uint size() const {
            return pieces.size();
        }

        /* Minimal amount of whitespace needed before the line bar `|`. */
        uint minTabSpace() const {
            if (line_no.has_value()) {
                return std::to_string(line_no.value()).size() + 1;
            } else {
                // Always at least one whitespace is displayed.
                return 1;
            }
        }

        std::vector<Highlight> print(uint tab_space, const std::map<uint, PointerMessage> &ctx, std::ostream &out) const {
            if (line_no.has_value()) {
                print_line_start(tab_space, line_no.value(), out);
            } else {
                print_line_start(tab_space, out);
            }

            // This is the relative column from the start of the code line.
            uint col = 0;

            std::vector<Highlight> lowered;
            std::vector<std::set<uint>> visited_groups(pieces.size(), std::set<uint>());

            for (uint i = 0; i < pieces.size(); ++i) {
                auto &piece = pieces[i];

                if (piece.getGroups().empty()) {
                    // Print the code piece.
                    out << piece.getText();
                    col += piece.getText().size();
                } else {
                    // Buffer all pointer messages.
                    for (auto group : piece.getGroups()) {
                        if (visited_groups[i].contains(group)) {
                            // This group on this piece has already been handled.
                            continue;
                        }

                        // Find all contigous pieces in this line and merge them.
                        uint last_idx = i;
                        uint last_col = col;
                        while (last_idx < pieces.size() && pieces[last_idx].getGroups().contains(group)) {
                            visited_groups[last_idx].insert(group);
                            last_col += pieces[last_idx].getText().size();
                            ++last_idx;
                        }
                        lowered.emplace_back(ctx.at(group).getPriority(), col, last_col, group, last_idx - 1, LoweringStage::None);
                    }
                    
                    // Print the code piece.
                    out << piece.getText();
                    col += piece.getText().size();
                }
            }
            out << '\n';
            return lowered;
        }
    };
}