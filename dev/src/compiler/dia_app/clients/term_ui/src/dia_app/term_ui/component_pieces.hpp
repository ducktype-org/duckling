#pragma once
#include <vector>
#include <set>
#include <proto/view.pb.h>

namespace term_ui {

    class TextPieces {
        std::vector<std::string> pieces;
    public:
        TextPieces(const view::NoHlComponent &component);

        std::string to_string() const;

    private:
        void add_component(const view::NoHlComponent &component);
    };

    class CodePiece {
        std::string text;
        std::set<uint> groups;
    public:
        CodePiece(std::string text);
        CodePiece(std::string text, std::set<uint> groups);
    
        CodePiece(const view::HlCodeComponent &component);

        const std::set<uint> &getGroups() const;

        const std::string &getText() const;

        std::string to_string() const;
    };

    class CodePieces {
        std::vector<CodePiece> pieces;
    public:
        CodePieces(const view::HlComponent &component);

        const std::vector<CodePiece> &getPieces() const;

        std::string to_string() const;

    private:
        void add_component(const view::HlComponent &component);
    };
}