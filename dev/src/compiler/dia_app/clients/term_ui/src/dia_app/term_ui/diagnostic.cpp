#include "diagnostic.hpp"

namespace term_ui {
    Diagnostic::Diagnostic(const view::Diagnostic &diag) {
        for (uint i = 0; i < diag.infos_size(); ++i) {
            infos.emplace_back(diag.infos(i));
        }
    }

    void Diagnostic::print(std::ostream& out) const {
        for (auto &info : infos) {
            info.print(out);
        }
    }
}