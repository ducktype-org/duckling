#pragma once
#include "diagnostic.hpp"

#define TERM_UI_SEPARATOR_WIDTH 80

namespace term_ui {
    class View {
        std::vector<Diagnostic> diags;
    public:
        View(const view::ViewResponse &view) {
            // In the static UI we ignore all side panel entries (view.side_paths(i)).
            for (uint i = 0; i < view.diagnostics_size(); ++i) {
                diags.emplace_back(view.diagnostics(i));
            }
        }

        void print(std::ostream& out, bool use_color) const {
            USE_COLOR = use_color;
            for (auto &diag : diags) {
                diag.print(out);
                out << std::string(TERM_UI_SEPARATOR_WIDTH, '-') << std::endl;
            }
        }
    };
}