#include "code_section.hpp"
#include <algorithm>
#include "diagnostic_interactive/term_ui/code_line.hpp"
#include "diagnostic_interactive/term_ui/highlight.hpp"
#include "diagnostic_interactive/term_ui/line.hpp"

namespace term_ui {

    namespace {
        enum class HighlightResult {
            LowerHighlight,
            LowerMessageMedium,
            LowerMessageLast,
            Success
        };

        HighlightResult fitLoweredMessage(
            Line& str, u64 beg, const dia_app::term_ui_view::PointerMessage& msg
        ) {
            if (!str.tryInsert(beg, intoLinePiece(msg, PointerStage::Message))) {
                str.tryInsert(beg, intoLinePiece(msg, PointerStage::Lowering));
                return HighlightResult::LowerMessageLast;
            }
            return HighlightResult::Success;
        }

        HighlightResult underline(
            Line& str,
            Highlight highlight,
            u64 line_no,
            const base::HashMap<u64, dia_app::term_ui_view::PointerMessage>& pointers,
            const base::HashMap<u64, std::pair<u64, u64>>& last_of_group
        ) {
            auto [priority, beg, end, group, idx, lowering] = highlight;
            u64   len                                       = end - beg;
            const auto& msg                                 = pointers.at(group);

            if (lowering == LoweringStage::Medium) {
                if (str.tryInsert(beg, intoLinePiece(msg, PointerStage::Lowering))) {
                    return HighlightResult::LowerMessageLast;
                } else {
                    return HighlightResult::LowerMessageMedium;
                }
            }
            if (lowering == LoweringStage::Last) {
                return fitLoweredMessage(str, beg, msg);
            }

            if (!str.isEmptyOn(beg, len)) {
                return HighlightResult::LowerHighlight;
            }

            if (std::make_pair(line_no, idx) != last_of_group.at(group)) {
                str.tryInsert(beg, intoLinePiece(msg, PointerStage::Highlight, len));
                return HighlightResult::Success;
            }

            if (!str.isEmptyOn(end, msg.text.size() + 2)) {
                str.tryInsert(beg, intoLinePiece(msg, PointerStage::HighlightWithLowering, len));
                return HighlightResult::LowerMessageMedium;
            }

            str.tryInsert(beg, intoLinePiece(msg, PointerStage::Highlight, len));
            str.tryInsert(end + 1, intoLinePiece(msg, PointerStage::Message));
            return HighlightResult::Success;
        }
    }

    void print(const dia_app::term_ui_view::CodeSection& section, std::ostream& out) {
        // Preprocessing
        base::HashMap<u64, std::pair<u64, u64>> last_of_group;
        u64 tab_space = 0;

        for (u64 l = 0; l < section.lines.size(); ++l) {
            const auto& line = section.lines[l];
            for (u64 i = 0; i < line.pieces.size(); ++i) {
                const auto& piece = line.pieces[i];
                if (!piece.pointer_ids.empty()) {
                    for (u64 group : piece.pointer_ids) {
                        last_of_group.put(group, { l, i });
                    }
                }
            }
            tab_space = std::max(tab_space, minTabSpace(line));
        }

        // Print location
        out << std::string(tab_space, ' ');
        out << "> " << section.file << ':' << section.line << ':' << section.col << '\n';

        print_line_start(tab_space, out);
        out << std::endl;

        for (u64 l = 0; l < section.lines.size(); ++l) {
            auto lowered = print(section.lines[l], tab_space, section.pointers, out);

            while (!lowered.empty()) {
                print_line_start(tab_space, out);

                auto this_lowered = std::move(lowered);
                lowered           = std::vector<Highlight>();

                std::sort(this_lowered.begin(), this_lowered.end());
                Line line;

                for (auto& highlight: this_lowered) {
                    switch (underline(line, highlight, l, section.pointers, last_of_group)) {
                    case HighlightResult::LowerHighlight: {
                        lowered.push_back(highlight);
                        break;
                    }
                    case HighlightResult::LowerMessageMedium: {
                        lowered.push_back(highlight.withStage(LoweringStage::Medium));
                        break;
                    }
                    case HighlightResult::LowerMessageLast: {
                        lowered.push_back(highlight.withStage(LoweringStage::Last));
                        break;
                    }
                    case HighlightResult::Success: {
                        break;
                    }
                    }
                }
                line.print(out);
            }
        }
    }
}
