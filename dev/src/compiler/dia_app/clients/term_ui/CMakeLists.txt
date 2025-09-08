add_executable(term_ui_test
    src/dia_app/term_ui/test.cpp
    src/dia_app/term_ui/code_line.hpp
    src/dia_app/term_ui/code_line.cpp
    src/dia_app/term_ui/code_section.hpp
    src/dia_app/term_ui/code_section.cpp
    src/dia_app/term_ui/component_pieces.hpp
    src/dia_app/term_ui/component_pieces.cpp
    src/dia_app/term_ui/diagnostic.hpp
    src/dia_app/term_ui/diagnostic.cpp
    src/dia_app/term_ui/highlight.hpp
    src/dia_app/term_ui/highlight.cpp
    src/dia_app/term_ui/info.hpp
    src/dia_app/term_ui/info.cpp
    src/dia_app/term_ui/line.hpp
    src/dia_app/term_ui/line.cpp
    src/dia_app/term_ui/styles.hpp
    src/dia_app/term_ui/styles.cpp
    src/dia_app/term_ui/view.hpp
    src/dia_app/term_ui/view.cpp
)

add_executable(term_ui
    src/dia_app/term_ui/code_line.hpp
    src/dia_app/term_ui/code_line.cpp
    src/dia_app/term_ui/code_section.hpp
    src/dia_app/term_ui/code_section.cpp
    src/dia_app/term_ui/component_pieces.hpp
    src/dia_app/term_ui/component_pieces.cpp
    src/dia_app/term_ui/diagnostic.hpp
    src/dia_app/term_ui/diagnostic.cpp
    src/dia_app/term_ui/highlight.hpp
    src/dia_app/term_ui/highlight.cpp
    src/dia_app/term_ui/info.hpp
    src/dia_app/term_ui/info.cpp
    src/dia_app/term_ui/line.hpp
    src/dia_app/term_ui/line.cpp
    src/dia_app/term_ui/main.cpp
    src/dia_app/term_ui/styles.hpp
    src/dia_app/term_ui/styles.cpp
    src/dia_app/term_ui/view.hpp
    src/dia_app/term_ui/view.cpp
)
target_link_libraries(term_ui PUBLIC JSON rang VIEW_MANAGER)

target_link_libraries(term_ui_test PUBLIC rang Proto)

add_library(TERM_UI
    src/dia_app/term_ui/code_line.hpp
    src/dia_app/term_ui/code_line.cpp
    src/dia_app/term_ui/code_section.hpp
    src/dia_app/term_ui/code_section.cpp
    src/dia_app/term_ui/component_pieces.hpp
    src/dia_app/term_ui/component_pieces.cpp
    src/dia_app/term_ui/diagnostic.hpp
    src/dia_app/term_ui/diagnostic.cpp
    src/dia_app/term_ui/highlight.hpp
    src/dia_app/term_ui/highlight.cpp
    src/dia_app/term_ui/info.hpp
    src/dia_app/term_ui/info.cpp
    src/dia_app/term_ui/line.hpp
    src/dia_app/term_ui/line.cpp
    src/dia_app/term_ui/styles.hpp
    src/dia_app/term_ui/styles.cpp
    src/dia_app/term_ui/view.hpp
    src/dia_app/term_ui/view.cpp
)

make_module(TERM_UI USES JSON rang Proto)