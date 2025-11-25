#pragma once
#include <diagnostic_interactive/core/term_ui_view.hpp>
#include <iostream>

namespace term_ui {
    void print(const dia_app::term_ui_view::Message& msg, std::ostream& out);
}
