#include "view.hpp"

namespace term_ui {
	View::View(const view::ViewResponse& view) {
		// In the static UI we ignore all side panel entries (view.side_paths(i)).
		for (u32 i = 0; i < view.diagnostics_size(); ++i) diags.emplace_back(view.diagnostics(i));
	}

	void View::print(std::ostream& out, bool use_color) const {
		// Set the global coloring flag.
		USE_COLOR = use_color;

		// Display the diagnostics.
		bool first_diag = true;
		for (auto& diag: diags) {
			if (!first_diag) {
				// Display a separator before every diagnostic except for
				// the first one.
				out << std::string(TERM_UI_SEPARATOR_WIDTH, '-') << std::endl;
			}
			diag.print(out);
			first_diag = false;
		}
	}
}
