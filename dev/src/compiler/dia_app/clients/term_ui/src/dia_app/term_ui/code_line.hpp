#pragma once
#include <optional>
#include "component_pieces.hpp"
#include "highlight.hpp"
#include "styles.hpp"

namespace term_ui {
    struct CodeLine {
        std::optional<uint> line_no;
        std::vector<CodePiece> pieces;

        CodeLine(uint line_no, std::vector<CodePiece> pieces);

        CodeLine(const view::CodeLine &line);

        CodePiece &operator[](const uint i);

        uint size() const;

        /* Minimal amount of whitespace needed before the line bar `|`. */
        uint minTabSpace() const;

        std::vector<Highlight> print(uint tab_space, const std::map<uint, PointerMessage> &ctx, std::ostream &out) const;
    };
}