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
            Location(const std::string &file, uint line, uint col);

            Location(const view::CodeMetadata &metadata);

            void print(uint tab_space, std::ostream &out) const;
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
        CodeSection(Location location, std::vector<CodeLine> lines, std::map<uint, PointerMessage> pointers);

        CodeSection(const view::CodeSection &section);

        void print(std::ostream &out) const;

    private:
        void computeLastOfAndTabSpace();

        HighlightResult fitLoweredMessage(Line &str, uint beg, const PointerMessage &msg) const;
        
        HighlightResult underline(Line &str, Highlight highlight, uint line_no) const;
    };

}