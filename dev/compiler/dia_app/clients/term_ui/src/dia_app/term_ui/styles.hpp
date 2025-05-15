#pragma once
#include <rang.hpp>

namespace term_ui {

    inline void reset_styles() {
        std::cerr << rang::fg::reset << rang::style::reset;
    }

    enum class StyleType {
        Error,
        Warning,
        Note,
        Hint,
        Docs
    };

    struct Style {
        std::string name;
        bool print_id;
        rang::fg color;
        rang::style style;
        rang::style main_text_style;
        char underline_char;
        char lowering_char;
        char lowering_attach_char;

        Style(std::string name, bool print_id, rang::fg color, rang::style style, rang::style main_text_style,
              char underline_char = ' ', char lowering_char = ' ', char lowering_attach_char = ' ') :
            name(name), print_id(print_id), color(color), style(style), main_text_style(main_text_style), underline_char(underline_char),
            lowering_char(lowering_char), lowering_attach_char(lowering_attach_char) {}
        
        void prepare() const {
            std::cerr << color << style;
        }

        void print_with(const std::string &text) const {
            prepare();
            std::cerr << text;
            reset_styles();
        }

        void prepare_main_text() const {
            std::cerr << main_text_style;
        }

        void print_main_with(const std::string &text) const {
            prepare_main_text();
            std::cerr << text;
            reset_styles();
        }

        void print_name(std::string id = "") const {
            prepare();
            std::cerr << name;
            if (print_id) {
                std::cerr << '[' << id << ']';
            }
            reset_styles();
        }
    };

    const Style ERROR_STYLE(
        "error",
        true,
        rang::fg::red,
        rang::style::bold,
        rang::style::bold,
        '^',
        '|',
        'Y'
    );
    const Style WARNING_STYLE(
        "warning",
        true,
        rang::fg::yellow,
        rang::style::bold,
        rang::style::bold,
        '^',
        '|',
        'Y'
    );
    const Style NOTE_STYLE(
        "note",
        false,
        rang::fg::blue,
        rang::style::bold,
        rang::style::reset,
        '~',
        '|',
        'v'
    );
    const Style HINT_STYLE(
        "hint",
        false,
        rang::fg::green,
        rang::style::bold,
        rang::style::reset,
        '+',
        '|',
        '+'
    );
    const Style DOCS_STYLE(
        "docs",
        false,
        rang::fg::cyan,
        rang::style::bold,
        rang::style::reset,
        '~',
        '|',
        '?'
    );

    const Style LINE_START_STYLE(
        "",
        false,
        rang::fg::reset,
        rang::style::bold,
        rang::style::reset
    );

    inline Style get_style(StyleType type) {
        switch(type) {
            case StyleType::Error: return ERROR_STYLE;
            case StyleType::Warning: return WARNING_STYLE;
            case StyleType::Note: return NOTE_STYLE;
            case StyleType::Hint: return HINT_STYLE;
            case StyleType::Docs: return DOCS_STYLE;
        }
        assert(false);
        return ERROR_STYLE;
    }
}