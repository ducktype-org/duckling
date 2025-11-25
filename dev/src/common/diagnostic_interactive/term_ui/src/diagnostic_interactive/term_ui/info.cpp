#include "info.hpp"
#include "code_section.hpp"
#include "diagnostic_interactive/term_ui/styles.hpp"

namespace term_ui {
    void print(const dia_app::term_ui_view::Message& msg, std::ostream& out) {
        Style style = get_style(msg.type);
        style.printName(msg.code, out);
        style.printWith(":", out);

        out << ' ';
        style.printMainWith(msg.header, out);
        out << '\n';

        for (auto& section : msg.sections) {
            if (std::holds_alternative<dia_app::term_ui_view::TextSection>(section)) {
                out << std::get<dia_app::term_ui_view::TextSection>(section) << '\n' << '\n';
            } else {
                print(std::get<dia_app::term_ui_view::CodeSection>(section), out);
                out << '\n';
            }
        }
    }
}
