#include "diagnostic.hpp"
#include "info.hpp"

namespace term_ui {
	void print(const dia_app::term_ui_view::Diagnostic& diag, std::ostream& out) {
		for (const auto& msg: diag.messages) { print(msg, out); }
	}
}
