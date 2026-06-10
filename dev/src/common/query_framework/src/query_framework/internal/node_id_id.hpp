#pragma once

#include <base/types/ints.hpp>
#include <vector> // for hash...
#include <limits>
#include <base/except/exceptions.hpp>
#include <base/collections/optional.hpp>

namespace query::internal {

    struct NodeID;

    class NodeIDID final {
        u64 id;

        friend class MaybeNodeIDID;

        explicit NodeIDID(u64 id): id(id) {}

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
 
    class MaybeNodeIDID final {
        u64 id;
        constexpr static u64 EMPTY_ID = std::numeric_limits<u64>::max();

    public:
        MaybeNodeIDID(): id(EMPTY_ID) {}
        MaybeNodeIDID(NodeIDID node_id_id): id(node_id_id.getInternalID()) {}
        MaybeNodeIDID(const MaybeNodeIDID&) = default;
        MaybeNodeIDID(MaybeNodeIDID&&) = default;
        MaybeNodeIDID& operator=(const MaybeNodeIDID&) = default;
        MaybeNodeIDID& operator=(MaybeNodeIDID&&) = default;


        [[nodiscard]]
        bool hasValue() const { return id != EMPTY_ID; }

        [[nodiscard]]
        bool empty() const { return id == EMPTY_ID; }

        [[nodiscard]]
        base::Optional<NodeIDID> asOptional() const {
            if (hasValue()) {
                return NodeIDID(id);
            }
            return {};
        }

        [[nodiscard]]
        NodeIDID getValue() const {
            CORE_ASSERT(hasValue(), "Trying to get value from empty MaybeNodeIDID");
            return NodeIDID(id);
        }
    };


    static_assert(sizeof(NodeIDID) == sizeof(u64), "NodeIDID should be the same size as u64");
    static_assert(sizeof(MaybeNodeIDID) == sizeof(u64), "MaybeNodeIDID should be the same size as u64");
}

template<>
struct std::hash<query::internal::NodeIDID> {
    std::size_t operator()(const query::internal::NodeIDID& k) const {
        return std::hash<u64>{}(k.getInternalID());
    }
};
