#include "view.hpp"

namespace term_ui {
    View::View(const view::ViewResponse &view) {
        // In the static UI we ignore all side panel entries (view.side_paths(i)).
        for (uint i = 0; i < view.diagnostics_size(); ++i) {
            diags.emplace_back(view.diagnostics(i));
        }
    }

    void View::print(std::ostream& out, bool use_color) const {
        USE_COLOR = use_color;
        for (auto &diag : diags) {
            diag.print(out);
            out << std::string(TERM_UI_SEPARATOR_WIDTH, '-') << std::endl;
        }
    }
}