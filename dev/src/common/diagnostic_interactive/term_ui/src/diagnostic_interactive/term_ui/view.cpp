#include "view.hpp"
#include "styles.hpp"

namespace term_ui {
    void print(const std::vector<dia_app::term_ui_view::Diagnostic>& diags, std::ostream& out, bool use_color) {
        // Set the global coloring flag.
        USE_COLOR = use_color;

        // Display the diagnostics.
        bool first_diag = true;
        for (const auto& diag : diags) {
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
