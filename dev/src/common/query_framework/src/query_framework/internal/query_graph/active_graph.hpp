#include "node_id.hpp"
#include <concurrent/collections/hash_map.hpp>


namespace query::internal {
    /**
     * Graph used to represent the set of active nodes and active edges in the query graph.
     * Its primary purpose is to store the count of currently executing queries and to detect query cycles. 
     *
     * @note Operations on this graph are thread safe, and should handle concurrnet cycle detection.
     */
    class ActiveGraph final {
        struct ActiveData final {

        };

        concurrent::ConHashMap<NodeID, ActiveData> active_nodes;


    };
}