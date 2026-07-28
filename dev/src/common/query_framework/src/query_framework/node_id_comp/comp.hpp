#include "../internal/query_graph/node_id.hpp"
#include <concurrent/base/collections/hash_map.hpp>


namespace query::internal {
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

    namespace {

        auto& getNodeIDToIDMapLocalCache() {
            static thread_local base::StableHashMap<NodeID, u64> node_id_to_id_map;
            return node_id_to_id_map;
        }

        auto& getIDToNodeIDMapLocalCache() {
            static thread_local base::StableHashMap<u64, NodeID> id_to_node_id_map;
            return id_to_node_id_map;
        }

        auto& getNodeIDToIDMap() {
            static concurrent::ConHashMap<NodeID, u64> node_id_to_id_map;
            return node_id_to_id_map;
        }

        auto& getIDToNodeIDMap() {
            static concurrent::ConHashMap<u64, NodeID> id_to_node_id_map;
            return id_to_node_id_map;
        }

        constinit std::atomic<u64> next_id{0};

        constexpr u64 MAX_ID = std::numeric_limits<u64>::max();
    }


    NodeIDID::NodeIDID(NodeID node_id) {

        // this->id = mapToKeyID(Key{
        //     .id = node_id.q_id.asInt(),
        //     .hash = {node_id.hash.val.data[0], node_id.hash.val.data[1], node_id.hash.val.data[2], node_id.hash.val.data[3]}
        // }).id;

        // return;

        auto& node_id_to_id_map_local = getNodeIDToIDMapLocalCache();
        if (auto id = node_id_to_id_map_local.atMaybeCopy(node_id)) {
            this->id = *id;
            return;
        }

        auto& node_id_to_id_map = getNodeIDToIDMap();
        auto& id_to_node_id_map = getIDToNodeIDMap();

        u64 my_id = 0;
        bool was_new = false;

        node_id_to_id_map.maybePutAndUpdate(node_id, MAX_ID, [&my_id, &was_new](Ref<u64> existing_id) {
            if (*existing_id != MAX_ID) {
                my_id = *existing_id;
            }
            else {
                my_id = next_id.fetch_add(1, std::memory_order_relaxed);
                *existing_id = my_id;
                was_new = true;
            }
        });

        this->id = my_id;

        // it didn't have it before, so we are save to put:
        node_id_to_id_map_local.put(node_id, my_id);
        getIDToNodeIDMapLocalCache().put(my_id, node_id);

        if (was_new) {
            id_to_node_id_map.put(my_id, node_id);
        }
    }

    NodeID NodeIDID::getID() const {
        auto& id_to_node_id_map_local = getIDToNodeIDMapLocalCache();
        if (auto node_id = id_to_node_id_map_local.atMaybeCopy(id)) {
            return *node_id;
        }

        // Btw this is not correct on its own, it might not find the node_id:
        auto& id_to_node_id_map = getIDToNodeIDMap();
        return id_to_node_id_map.getCopy(id);

        // KeyID key_id {.id = id};
        // auto org =  key_id.getOriginalKey();

        // return NodeID{QueryID(org.id), KeyHash{base::Bit256(org.hash[0], org.hash[1], org.hash[2], org.hash[3])}};

    }
}

// NodeIDID fromNodeID(const query::NodeID& node_id) {
//     // ...
// }