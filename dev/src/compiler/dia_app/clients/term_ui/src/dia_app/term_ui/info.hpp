#pragma once
#include <variant>
#include "code_section.hpp"
#include "component_pieces.hpp"

namespace term_ui {

    struct Info {
        using TextSection = std::string;
        using TextOrCode = std::variant<TextSection, CodeSection>;

        StyleType type;
        uint id;
        TextSection main_section;
        std::vector<TextOrCode> sections;

        // Constructor for testing purposes only.
        Info(StyleType type, uint id, const std::string &message, const std::string &description,
                const CodeSection &code);
        
        Info(const view::Info &info);

        void print(std::ostream& out) const;
    };

}