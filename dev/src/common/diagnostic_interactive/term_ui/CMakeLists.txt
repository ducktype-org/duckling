
add_executable(term_ui
    src/diagnostic_interactive/term_ui/code_line.hpp
    src/diagnostic_interactive/term_ui/code_line.cpp
    src/diagnostic_interactive/term_ui/code_section.hpp
    src/diagnostic_interactive/term_ui/code_section.cpp
    src/diagnostic_interactive/term_ui/diagnostic.hpp
    src/diagnostic_interactive/term_ui/diagnostic.cpp
    src/diagnostic_interactive/term_ui/highlight.hpp
    src/diagnostic_interactive/term_ui/highlight.cpp
    src/diagnostic_interactive/term_ui/info.hpp
    src/diagnostic_interactive/term_ui/info.cpp
    src/diagnostic_interactive/term_ui/line.hpp
    src/diagnostic_interactive/term_ui/line.cpp
    src/diagnostic_interactive/term_ui/main.cpp
    src/diagnostic_interactive/term_ui/styles.hpp
    src/diagnostic_interactive/term_ui/styles.cpp
    src/diagnostic_interactive/term_ui/view.hpp
    src/diagnostic_interactive/term_ui/view.cpp
)
target_link_libraries(term_ui PUBLIC JSON rang Base DiagnosticInteractiveCore)

add_library(TERM_UI
    src/diagnostic_interactive/term_ui/code_line.hpp
    src/diagnostic_interactive/term_ui/code_line.cpp
    src/diagnostic_interactive/term_ui/code_section.hpp
    src/diagnostic_interactive/term_ui/code_section.cpp
    src/diagnostic_interactive/term_ui/diagnostic.hpp
    src/diagnostic_interactive/term_ui/diagnostic.cpp
    src/diagnostic_interactive/term_ui/highlight.hpp
    src/diagnostic_interactive/term_ui/highlight.cpp
    src/diagnostic_interactive/term_ui/info.hpp
    src/diagnostic_interactive/term_ui/info.cpp
    src/diagnostic_interactive/term_ui/line.hpp
    src/diagnostic_interactive/term_ui/line.cpp
    src/diagnostic_interactive/term_ui/styles.hpp
    src/diagnostic_interactive/term_ui/styles.cpp
    src/diagnostic_interactive/term_ui/view.hpp
    src/diagnostic_interactive/term_ui/view.cpp
)

make_module(TERM_UI USES DiagnosticInteractiveCore rang)