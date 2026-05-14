

add_library(TerminalUI
    src/diagnostic_interactive/term_ui/code_line.hpp
    src/diagnostic_interactive/term_ui/code_line.cpp
    src/diagnostic_interactive/term_ui/code_section.hpp
    src/diagnostic_interactive/term_ui/code_section.cpp
    src/diagnostic_interactive/term_ui/highlight.hpp
    src/diagnostic_interactive/term_ui/highlight.cpp
    src/diagnostic_interactive/term_ui/line.hpp
    src/diagnostic_interactive/term_ui/line.cpp
    src/diagnostic_interactive/term_ui/module_flags/module_flags.hpp
    src/diagnostic_interactive/term_ui/module_flags/module_flags.cpp
    src/diagnostic_interactive/term_ui/styles.hpp
    src/diagnostic_interactive/term_ui/styles.cpp
    src/diagnostic_interactive/term_ui/printers.hpp
    src/diagnostic_interactive/term_ui/printers.cpp
)

make_module(TerminalUI USES DiagnosticInteractiveCore rang)


duck_add_test(common term_ui tests/term_ui_test.cpp USES TerminalUI )
