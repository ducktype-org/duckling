#pragma once

#include <iostream>
#include <vector>
#include <set>
#include <assert.h>
#include "utils.hpp"
#include "styles.hpp"

namespace term_ui {

    /*
        Note: numbers refer the to order of evalutation of underlinings
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
        None = 0, // Has not printed the underlining yet.
        Medium = 2, // At least one lowering_char must yet be displayed.
        Last = 1 // Here comes the message.
    };

    struct Underlining {
        uint beg, end;
        uint group;
        uint idx; // Index of the last underlined code piece.
        LoweringStage lowering;

        Underlining(uint beg, uint end, uint group, uint idx, LoweringStage stage) :
            beg(beg), end(end), group(group), idx(idx), lowering(stage) {}

        bool operator<(const Underlining &other) const {
            if (end == other.end) {
                if (lowering == other.lowering) {
                    return group < other.group;
                }
                return lowering < other.lowering;
            }
            return end > other.end;
        }

        Underlining with_stage(LoweringStage stage) const {
            switch (stage) {
                case LoweringStage::None: {
                    return *this;
                }
                case LoweringStage::Medium: {
                    return Underlining(beg, beg + 1, group, idx, stage);
                }
                case LoweringStage::Last: {
                    return Underlining(beg, beg + 1, group, idx, stage);
                }
            }
            return *this;
        }
    };

    enum class PointerStage {
        Underline,
        UnderlineWithLowering,
        Lowering,
        Message
    };

    struct PointerMessage {
        const std::string text;
        const StyleType type;
        
        PointerMessage(std::string text, StyleType type) :
            text(text), type(type) {}

        LinePiece into_line_piece(PointerStage stage, int count = -1) const {
            std::string str;
            Style style = get_style(type);
            switch (stage) {
                case PointerStage::Underline: {
                    assert(count > 0);
                    str = std::string(count, style.underline_char);
                    break;
                }
                case PointerStage::UnderlineWithLowering: {
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

    struct CodePiece {
        std::string text;
        std::set<uint> groups;

        CodePiece(std::string text) : text(text) {}
        CodePiece(std::string text, std::set<uint> groups) : text(text), groups(groups) {}
    };

    struct CodeLine {
        // Line number. If 0, no line number printed.
        uint line_no;
        vec<CodePiece> pieces;

        CodeLine(uint line_no, vec<CodePiece> pieces) : line_no(line_no), pieces(pieces) {}

        CodePiece &operator[](const uint i) {
            return pieces[i];
        }

        uint size() const {
            return pieces.size();
        }

        /* Minimal amount of whitespace needed before the line bar `|`. */
        uint min_tab_space() const {
            return std::to_string(line_no).size() + 1;
        }

        vec<Underlining> print(uint tab_space) const {
            if (line_no > 0) {
                print_line_start(tab_space, line_no);
            } else {
                print_line_start(tab_space);
            }

            // This is the perceived column from the start of the code line.
            uint col = 0;

            vec<Underlining> lowered;
            vec<std::set<uint>> visited_groups(pieces.size(), std::set<uint>());

            for (uint i = 0; i < pieces.size(); ++i) {
                auto &piece = pieces[i];

                if (piece.groups.empty()) {
                    // Print the code piece.
                    std::cout << piece.text;
                    col += piece.text.size();
                } else {
                    // Buffer all pointer messages.
                    for (auto group : piece.groups) {
                        if (visited_groups[i].contains(group)) {
                            // This group on this piece has already been handled.
                            continue;
                        }

                        // Find all contigous pieces in this line and merge them.
                        uint last_idx = i;
                        uint last_col = col;
                        while (last_idx < pieces.size() && pieces[last_idx].groups.contains(group)) {
                            visited_groups[last_idx].insert(group);
                            last_col += pieces[last_idx].text.size();
                            ++last_idx;
                        }
                        lowered.emplace_back(col, last_col, group, last_idx - 1, LoweringStage::None);
                    }
                    
                    // Print the code piece.
                    std::cout << piece.text;
                    col += piece.text.size();
                }
            }

            std::cout << std::endl;
            return lowered;
        }
    };

    struct CodeFragment {
        struct Location {
            std::string file;
            uint line, col;

            Location(const std::string &file, uint line, uint col) : file(file), line(line), col(col) {}

            void print(uint tab_space) const {
                std::cout << std::string(tab_space, ' ');
                std::cout << "/ " << file << ':' << line << ':' << col << " /" << std::endl;
            }
        };
        Location location;
        vec<CodeLine> lines;
        vec<PointerMessage> pointers;
        vec<std::pair<int, int>> last_of_group;
        uint tab_space;
        
        enum class UnderlineResult {
            LowerUnderlining,
            LowerMessageMedium,
            LowerMessageLast,
            Success
        };

        CodeFragment(Location location, vec<CodeLine> lines, vec<PointerMessage> pointers) :
            location(location), lines(lines), pointers(pointers), tab_space(0) {
            // For each group find the last of its pieces.
            // That's where the message will appear.
            last_of_group.assign(pointers.size(), {-1, -1});
            for (uint l = 0; l < lines.size(); ++l) {
                auto &line = lines[l];

                for (uint i = 0; i < line.size(); ++i) {
                    auto &piece = line[i];
                    if (!piece.groups.empty()) {
                        for (uint group : piece.groups) {
                            last_of_group[group] = {l, i};
                        }
                    }
                }
                tab_space = std::max(tab_space, line.min_tab_space());
            }
        }

        UnderlineResult fit_lowered_message(Line &str, uint beg, const PointerMessage &msg) const {
            if (!str.try_insert(beg, msg.into_line_piece(PointerStage::Message))) {
                // The message does not fit.
                str.try_insert(beg, msg.into_line_piece(PointerStage::Lowering));
                return UnderlineResult::LowerMessageLast;
            }
            // The message fits.
            return UnderlineResult::Success;
        }
        
        UnderlineResult underline(Line &str, Underlining underlining, uint line_no) const {
            auto [beg, end, group, idx, lowering] = underlining;
            uint len = end - beg;
            auto &msg = pointers[group];

            // Handle lowering stages first.
            if (lowering == LoweringStage::Medium) {
                if (str.try_insert(beg, msg.into_line_piece(PointerStage::Lowering))) {
                    return UnderlineResult::LowerMessageLast;
                } else {
                    // A lowering char must yet be displayed.
                    return UnderlineResult::LowerMessageMedium;
                }
            }
            if (lowering == LoweringStage::Last) {
                return fit_lowered_message(str, beg, msg);
            }
            // There has been no lowering yet.

            // Check if the underlining can be placed in this line.
            if (!str.is_empty_on(beg, len)) {
                // It cannot.
                return UnderlineResult::LowerUnderlining;
            }

            if (std::make_pair((int)line_no, (int)idx) != last_of_group[group]) {
                // No message attached, just underline.
                str.try_insert(beg, msg.into_line_piece(PointerStage::Underline, len));
                return UnderlineResult::Success;
            }

            // Check if the message can be placed in this line.
            // ............... message ......
            //                ^^^^^^^^^ these spots need to be free
            //                ^ end
            //                         ^ end + msg.text.size() + 2
            if (!str.is_empty_on(end, msg.text.size() + 2)) {
                // We already know we can underline, do so with lowering.
                str.try_insert(beg, msg.into_line_piece(PointerStage::UnderlineWithLowering, len));
                return UnderlineResult::LowerMessageMedium;
            }

            // Both the underlining and the message fit.
            str.try_insert(beg, msg.into_line_piece(PointerStage::Underline, len));
            str.try_insert(end + 1, msg.into_line_piece(PointerStage::Message));
            return UnderlineResult::Success;
        }

        void print() const {
            location.print(tab_space);
            print_line_start(tab_space);
            std::cout << std::endl;
            for (uint l = 0; l < lines.size(); ++l) {
                auto lowered = lines[l].print(tab_space);
                
                // Handle buffered pointer messages.
                while (!lowered.empty()) {
                    print_line_start(tab_space);

                    auto this_lowered = std::move(lowered);
                    lowered = vec<Underlining>();

                    std::sort(this_lowered.begin(), this_lowered.end());
                    Line line;

                    for (auto &underlining : this_lowered) {
                        uint beg = underlining.beg;
                        switch (underline(line, underlining, l)) {
                            case UnderlineResult::LowerUnderlining: {
                                // The underlining did not fit.
                                lowered.push_back(underlining);
                                break;
                            }
                            case UnderlineResult::LowerMessageMedium: {
                                // The message did not fit.
                                lowered.push_back(underlining.with_stage(LoweringStage::Medium));
                                break;
                            }
                            case UnderlineResult::LowerMessageLast: {
                                // Lowered message's lowering char has been
                                // displayed.
                                lowered.push_back(underlining.with_stage(LoweringStage::Last));
                                break;
                            }
                            case UnderlineResult::Success: {
                                // The message has been displayed.
                                break;
                            }
                        }
                    }

                    line.print();
                }
            }
        }
    };

}