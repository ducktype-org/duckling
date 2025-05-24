add_executable(term_ui_test
    src/dia_app/term_ui/test.cpp
)

target_link_libraries(term_ui_test PUBLIC rang Proto)

add_library(TERM_UI
    src/dia_app/term_ui/code_section.hpp
    src/dia_app/term_ui/code_line.hpp
    src/dia_app/term_ui/component_pieces.hpp
    src/dia_app/term_ui/diagnostic.hpp
    src/dia_app/term_ui/highlight.hpp
    src/dia_app/term_ui/info.hpp
    src/dia_app/term_ui/line.hpp
    src/dia_app/term_ui/styles.hpp
    src/dia_app/term_ui/view.hpp
)

make_module(TERM_UI USES JSON rang Proto)