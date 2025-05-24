#pragma once
#include "info.hpp"

namespace term_ui {
    class Diagnostic {
        std::vector<Info> infos;
    public:
        Diagnostic(const view::Diagnostic &diag) {
            for (uint i = 0; i < diag.infos_size(); ++i) {
                infos.emplace_back(diag.infos(i));
            }
        }

        void print(std::ostream& out) const {
            for (auto &info : infos) {
                info.print(out);
            }
        }
    };
}