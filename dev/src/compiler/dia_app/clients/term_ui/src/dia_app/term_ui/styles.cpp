#include "styles.hpp"

namespace term_ui {
    bool USE_COLOR = true;

    const Style ERROR_STYLE(
        "error",
        "E",
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
        "W",
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
        "N",
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
        "H",
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
        "D",
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
        "",
        false,
        rang::fg::reset,
        rang::style::bold,
        rang::style::reset
    );
}