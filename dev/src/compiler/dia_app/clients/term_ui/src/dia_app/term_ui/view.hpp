#pragma once
#include "diagnostic.hpp"

#define TERM_UI_SEPARATOR_WIDTH 80

namespace term_ui {
    class View {
        std::vector<Diagnostic> diags;
    public:
        View(const view::ViewResponse &view);

        void print(std::ostream& out, bool use_color) const;
    };
}