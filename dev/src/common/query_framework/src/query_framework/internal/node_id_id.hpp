#pragma once

#include "query_graph/node_id.hpp"

namespace query::internal {

    class NodeIDID final {
        u64 id;

    public:
        NodeIDID() = delete;

        NodeIDID(const NodeIDID&) = default;
        NodeIDID(NodeIDID&&) = default;
        NodeIDID& operator=(const NodeIDID&) = default;
        NodeIDID& operator=(NodeIDID&&) = default;

        explicit NodeIDID(NodeID id);

        [[nodiscard]]
        NodeID getID() const;
    };
 
    // + optimized nodeidid -> key concurrent map
    // for dense cases (e.g. task pool or active graph)
}

