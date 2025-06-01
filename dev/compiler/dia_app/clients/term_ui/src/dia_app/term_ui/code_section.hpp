#pragma once

#include <iostream>
#include <vector>
#include <set>
#include <assert.h>
#include <proto/view.pb.h>
#include "styles.hpp"
#include "highlight.hpp"
#include "line.hpp"
#include "code_line.hpp"

namespace term_ui {

    class CodeSection {
    public:
        class Location {
            std::string file;
            uint line, col;
        public:
            Location(const std::string &file, uint line, uint col) : file(file), line(line), col(col) {}

            Location(const view::CodeMetadata &metadata) :
                file(metadata.filename()), line(metadata.line()), col(metadata.column()) {}

            void print(uint tab_space, std::ostream &out) const {
                out << std::string(tab_space, ' ');
                out << "> " << file << ':' << line << ':' << col << '\n';
            }
        };
    private:
        Location location;
        std::vector<CodeLine> lines;
        std::map<uint, PointerMessage> pointers;
        // last_of_group[group_id] = {line_no, col_no}.
        std::map<uint, std::pair<uint, uint>> last_of_group;
        uint tab_space;
        
        enum class HighlightResult {
            LowerHighlight,
            LowerMessageMedium,
            LowerMessageLast,
            Success
        };

    public:
        CodeSection(Location location, std::vector<CodeLine> lines, std::map<uint, PointerMessage> pointers) :
            location(location), lines(lines), pointers(pointers), tab_space(0) {
            computeLastOfAndTabSpace();
        }

        CodeSection(const view::CodeSection &section) :
            location(section.metadata()), tab_space(0) {
            // Extract lines.
            for (uint i = 0; i < section.lines_size(); ++i) {
                lines.emplace_back(section.lines(i));
            }
            // Extract pointers.
            for (uint i = 0; i < section.hl_messages_size(); ++i) {
                auto &ptr = section.hl_messages(i);
                pointers.insert({ptr.tag(), PointerMessage(ptr)});
            }
            // Perform pre-processing.
            computeLastOfAndTabSpace();
        }

        void print(std::ostream &out) const {
            location.print(tab_space, out);
            print_line_start(tab_space, out);
            out << std::endl;
            for (uint l = 0; l < lines.size(); ++l) {
                auto lowered = lines[l].print(tab_space, pointers, out);
                
                // Handle buffered pointer messages.
                while (!lowered.empty()) {
                    print_line_start(tab_space, out);

                    auto this_lowered = std::move(lowered);
                    lowered = std::vector<Highlight>();

                    std::sort(this_lowered.begin(), this_lowered.end());
                    Line line;

                    for (auto &highlight : this_lowered) {
                        uint beg = highlight.beg;
                        switch (underline(line, highlight, l)) {
                            case HighlightResult::LowerHighlight: {
                                // The highlight did not fit.
                                lowered.push_back(highlight);
                                break;
                            }
                            case HighlightResult::LowerMessageMedium: {
                                // The message did not fit.
                                lowered.push_back(highlight.withStage(LoweringStage::Medium));
                                break;
                            }
                            case HighlightResult::LowerMessageLast: {
                                // Lowered message's lowering char has been
                                // displayed.
                                lowered.push_back(highlight.withStage(LoweringStage::Last));
                                break;
                            }
                            case HighlightResult::Success: {
                                // The message has been displayed.
                                break;
                            }
                        }
                    }
                    line.print(out);
                }
            }
        }

    private:
        void computeLastOfAndTabSpace() {
            // For each group find the last of its pieces.
            // That's where the message will appear.
            for (uint l = 0; l < lines.size(); ++l) {
                auto &line = lines[l];

                for (uint i = 0; i < line.size(); ++i) {
                    auto &piece = line[i];
                    if (!piece.getGroups().empty()) {
                        for (uint group : piece.getGroups()) {
                            last_of_group[group] = {l, i};
                        }
                    }
                }
                tab_space = std::max(tab_space, line.minTabSpace());
            }
        }

        HighlightResult fitLoweredMessage(Line &str, uint beg, const PointerMessage &msg) const {
            if (!str.tryInsert(beg, msg.intoLinePiece(PointerStage::Message))) {
                // The message does not fit.
                str.tryInsert(beg, msg.intoLinePiece(PointerStage::Lowering));
                return HighlightResult::LowerMessageLast;
            }
            // The message fits.
            return HighlightResult::Success;
        }
        
        HighlightResult underline(Line &str, Highlight highlight, uint line_no) const {
            auto [priority, beg, end, group, idx, lowering] = highlight;
            uint len = end - beg;
            auto &msg = pointers.at(group);

            // Handle lowering stages first.
            if (lowering == LoweringStage::Medium) {
                if (str.tryInsert(beg, msg.intoLinePiece(PointerStage::Lowering))) {
                    return HighlightResult::LowerMessageLast;
                } else {
                    // A lowering char must yet be displayed.
                    return HighlightResult::LowerMessageMedium;
                }
            }
            if (lowering == LoweringStage::Last) {
                return fitLoweredMessage(str, beg, msg);
            }
            // There has been no lowering yet.

            // Check if the highlight can be placed in this line.
            if (!str.isEmptyOn(beg, len)) {
                // It cannot.
                return HighlightResult::LowerHighlight;
            }

            if (std::make_pair(line_no, idx) != last_of_group.at(group)) {
                // No message attached, just underline.
                str.tryInsert(beg, msg.intoLinePiece(PointerStage::Highlight, len));
                return HighlightResult::Success;
            }

            // Check if the message can be placed in this line.
            // ............... message ......
            //                ^^^^^^^^^ these spots need to be free
            //                ^ end
            //                         ^ end + msg.text.size() + 2
            if (!str.isEmptyOn(end, msg.getText().size() + 2)) {
                // We already know we can underline, do so with lowering.
                str.tryInsert(beg, msg.intoLinePiece(PointerStage::HighlightWithLowering, len));
                return HighlightResult::LowerMessageMedium;
            }

            // Both the highlight and the message fit.
            str.tryInsert(beg, msg.intoLinePiece(PointerStage::Highlight, len));
            str.tryInsert(end + 1, msg.intoLinePiece(PointerStage::Message));
            return HighlightResult::Success;
        }
    };

}