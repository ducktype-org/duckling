#pragma once
#include "info.hpp"

namespace term_ui {
    class Diagnostic {
        std::vector<Info> infos;
    public:
        Diagnostic(const view::Diagnostic &diag);

        void print(std::ostream& out) const;
    };
}