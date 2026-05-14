#include "printers.hpp"

#include <diagnostic_interactive/term_ui/code_section.hpp>
#include <diagnostic_interactive/term_ui/module_flags/module_flags.hpp>
#include <diagnostic_interactive/term_ui/styles.hpp>

namespace term_ui {
	void print(const dia_int::term_ui_view::Message& msg, std::ostream& out) {
		Style style = getStyleFromType(msg.type);
		style.printName(msg.code, out);
		style.printWith(":", out);

		out << ' ';
		style.printMainWith(msg.header, out);
		out << '\n';

		for (auto& section: msg.sections) {
			if (std::holds_alternative<dia_int::term_ui_view::TextSection>(section)) {
				out << std::get<dia_int::term_ui_view::TextSection>(section) << '\n' << '\n';
			} else {
				print(std::get<dia_int::term_ui_view::CodeSection>(section), out);
				out << '\n';
			}
		}
	}

	void print(const dia_int::term_ui_view::Diagnostic& diag, std::ostream& out) {
		for (const auto& msg: diag.messages) print(msg, out);
	}

	void print(
		const std::vector<dia_int::term_ui_view::Diagnostic>& diags,
		std::ostream&                                         out,
		bool                                                  use_color_local
	) {
		// Set the global coloring flag.
		configureColoring(use_color_local);

		// Display the diagnostics.
		bool first_diag = true;
		for (const auto& diag: diags) {
			if (!first_diag) {
				// Display a separator before every diagnostic except for
				// the first one.
				out << std::string(TERM_UI_SEPARATOR_WIDTH, '-') << '\n';
			}
			print(diag, out);
			first_diag = false;
		}
	}
}
