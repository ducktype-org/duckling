#include "comp.hpp"
#include <iostream>
#include <random>
#include <thread>
#include <vector>

constexpr u64 THREAD_COUNT = 4;
constexpr u64 OP_COUNT = 1'000'000;


// Random u64 generator:
thread_local std::mt19937_64 rng(int(std::random_device{}()));
u64 getU64() {
    return rng();
}

query::internal::NodeID getRandomNodeID() {
    return query::internal::NodeID{
        query::internal::QueryID::createUnregisteredForTests(getU64()),
        query::internal::KeyHash{base::Bit256(getU64(), getU64(), getU64(), getU64())}
    };
}
// query::Node


int main( ) {

    std::vector<std::jthread> threads;

    std::vector<query::internal::NodeID> node_id_pool;
    for (u64 i = 0; i < OP_COUNT; ++i) {
        node_id_pool.push_back(getRandomNodeID());
    }

    auto get_from_pool = [&node_id_pool]() -> query::internal::NodeID {
        u64 i = getU64() % node_id_pool.size();
        return node_id_pool.at(i);;
    };

    for (u64 i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back([i, &node_id_pool, &get_from_pool]() {
            for (u64 j = 0; j < OP_COUNT; ++j) {
                auto node_id = get_from_pool();


                auto node_id_id = query::internal::NodeIDID(node_id);
                auto node_id_back = node_id_id.getID();

                if (!(node_id == node_id_back)) {
                    std::cerr << "Mismatch: " << std::endl;
                    std::cerr << "Original: " << node_id.q_id.asInt() << ", " << node_id.hash.val.data[0] << ", " << node_id.hash.val.data[1] << ", " << node_id.hash.val.data[2] << ", " << node_id.hash.val.data[3] << std::endl;
                    std::cerr << "Back:     " << node_id_back.q_id.asInt() << ", " << node_id_back.hash.val.data[0] << ", " << node_id_back.hash.val.data[1] << ", " << node_id_back.hash.val.data[2] << ", " << node_id_back.hash.val.data[3] << std::endl;
                    throw std::runtime_error("Mismatch");
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

}