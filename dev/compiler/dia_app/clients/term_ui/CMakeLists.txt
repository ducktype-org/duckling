add_executable(term_ui_test
    src/dia_app/term_ui/test.cpp
)

target_link_libraries(term_ui_test PUBLIC rang Proto)

add_library(TERM_UI
    src/dia_app/term_ui/code_fragment.hpp
    src/dia_app/term_ui/message.hpp
    src/dia_app/term_ui/styles.hpp
    src/dia_app/term_ui/utils.hpp
)

make_module(TERM_UI USES JSON rang Proto)