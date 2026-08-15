

add_library(TerminalUI
    src/diagnostic/term_ui/code_line.hpp
    src/diagnostic/term_ui/code_line.cpp
    src/diagnostic/term_ui/code_section.hpp
    src/diagnostic/term_ui/code_section.cpp
    src/diagnostic/term_ui/highlight.hpp
    src/diagnostic/term_ui/highlight.cpp
    src/diagnostic/term_ui/line.hpp
    src/diagnostic/term_ui/line.cpp
    src/diagnostic/term_ui/module_flags/module_flags.hpp
    src/diagnostic/term_ui/module_flags/module_flags.cpp
    src/diagnostic/term_ui/styles.hpp
    src/diagnostic/term_ui/styles.cpp
    src/diagnostic/term_ui/printers.hpp
    src/diagnostic/term_ui/printers.cpp
)

make_module(TerminalUI USES DiagnosticCore rang)


duck_add_test(common term_ui tests/term_ui_test.cpp USES TerminalUI )
