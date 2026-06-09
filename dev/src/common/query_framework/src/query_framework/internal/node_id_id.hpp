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

        bool operator==(const NodeIDID& other) const = default;
        bool operator!=(const NodeIDID& other) const = default;
        bool operator<(const NodeIDID& other) const { return id < other.id; }

        explicit NodeIDID(NodeID id);

        [[nodiscard]]
        NodeID getID() const;

        [[nodiscard]]
        u64 getInternalID() const { return id; }
    };
 
    // + optimized nodeidid -> key concurrent map
    // for dense cases (e.g. task pool or active graph)
}

template<>
struct std::hash<query::internal::NodeIDID> {
    std::size_t operator()(const query::internal::NodeIDID& k) const {
        return std::hash<u64>{}(k.getInternalID());
    }
};
