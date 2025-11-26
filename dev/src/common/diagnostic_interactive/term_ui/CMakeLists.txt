

add_library(TerminalUI
    src/diagnostic_interactive/term_ui/code_line.hpp
    src/diagnostic_interactive/term_ui/code_line.cpp
    src/diagnostic_interactive/term_ui/code_section.hpp
    src/diagnostic_interactive/term_ui/code_section.cpp
    src/diagnostic_interactive/term_ui/highlight.hpp
    src/diagnostic_interactive/term_ui/highlight.cpp
    src/diagnostic_interactive/term_ui/line.hpp
    src/diagnostic_interactive/term_ui/line.cpp
    src/diagnostic_interactive/term_ui/styles.hpp
    src/diagnostic_interactive/term_ui/styles.cpp
    src/diagnostic_interactive/term_ui/printers.hpp
    src/diagnostic_interactive/term_ui/printers.cpp
)

make_module(TerminalUI USES DiagnosticInteractiveCore rang)


add_executable(term_ui_test
    tests/test.cpp
)
target_link_libraries(term_ui_test PUBLIC TerminalUI JSON rang Base DiagnosticInteractiveCore)

add_executable(term_ui_demo
    src/diagnostic_interactive/term_ui/main.cpp
)
target_link_libraries(term_ui_demo PUBLIC TerminalUI JSON DiagnosticInteractiveCore Base)
