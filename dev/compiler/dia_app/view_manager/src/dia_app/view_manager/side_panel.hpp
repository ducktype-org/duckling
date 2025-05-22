#pragma once
#include "components.hpp"

namespace dia_app {
namespace view_manager {
    using edge_id_t = uint32_t;

    class SidePath {
        private:
        // TODO - sprawdzić jak to się w ogóle parsuje

        public:
        std::unique_ptr<::view::SidePath> getView() const;
    };

    class SideEdge {

    };

    class SideInfo {

    };
} // namespace view_manager
} // namespace dia_app