

#include "node_id_id.hpp"
#include <concurrent/base/collections/hash_map.hpp>

namespace query::internal {

    namespace {
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

        if (was_new) {
            id_to_node_id_map.put(my_id, node_id);
        }
    }

    NodeID NodeIDID::getID() const {
        auto& id_to_node_id_map = getIDToNodeIDMap();
        return id_to_node_id_map.getCopy(id);
    }

}

