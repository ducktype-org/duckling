#include <base/stable_hashmap.hpp>
#include <base/maps.hpp>
#include <concurrent/concurrent_allocator_take_2.hpp>
#include <iostream>


template<class K, class T>
class MyHashMap {
    // @EH: this still perform alloc per element (argh!)
    base::HashMap<K, Ref<T>> map;
    concurrent::SingleThreadedAllocator<T, 2048> allocator;

public:
    MyHashMap() = default;
    void put(const K& key, const T& value) {
        auto item = allocator.allocateEmplace(value);
        map.put(key, item);
    }
    base::Optional<Ref<T>> atMaybe(const K& key) {
        auto maybe_ref =  map.atMaybe(key);
        if (maybe_ref) {
            return *maybe_ref.value();
        } else {
            return {};
        }
    }
};

// assume u64 keys
template<class T>
class MyCustomHashMap final {
    static constexpr usize INITIAL_BUCKETS = 16;
    static constexpr double MAX_LOAD_FACTOR = 0.5;
    using Key = u64;
    
    struct Node final {
        Key key;
        T value;
        MRef<Node> next;
    };

    std::vector<MRef<Node>> buckets;
    concurrent::SingleThreadedAllocator<Node, 2048> node_allocator;
    u64 element_count = 0;

    [[nodiscard]]
    u64 keyToBucket(const Key& key) const {
        return key % buckets.size();
    }

    void rehash() {
        usize new_bucket_count = buckets.size() * 2;
        std::vector<Ref<Node>> all_nodes;
        for (const auto& bucket : buckets) {
            for (const auto& node : bucket) {
                all_nodes.push_back(node);
            }
        }
        buckets.clear();
        buckets.resize(new_bucket_count);

        
        for (const auto& node : all_nodes) {
            u64 new_bucket_size = 0;
            MRef maybe_initial_node = buckets.at(keyToBucket(node.key));
            if (maybe_initial_node) {
                new_bucket_size = addToBucket(maybe_initial_node.toOpt().value(), node);
            } else {
                buckets.at(keyToBucket(node.key)) = node;
                new_bucket_size = 1;
            }
        }
        
    }

    /**
     * Appends new node to the bucket identified by node reference.
     * Returns new size of the bucket.
     * Checks if a node with the same key already exists - if so, throws logic_error.
     */
    void addToBucket(MRef<Node> bucket, Ref<Node> new_node) {
        if (bucket) {
            Ref current_node = bucket;
            Key key = new_node->key;

            while (true) {
                if (current_node.key == key) {
                    // this can be changed to an assertion:
                    throw std::logic_error("Duplicate key insertion in MyCustomHashMap");
                }
                if (current_node->next) {
                    current_node = current_node->next.toOpt().value();
                } else {
                    break;
                }
            }
            current_node->next = new_node;
        }

        
    }

    void maybeRehash() {
        if (load_factor > buckets.size()) {
            rehash();
        }
    }

public:
    MyCustomHashMap() : buckets(INITIAL_BUCKETS) {}

    void put(const Key& key, const T& value) {
        MRef maybe_initial_node = buckets.at(keyToBucket(key));
        u64 new_bucket_size = 0;

        if (maybe_initial_node) {
            Ref new_node = node_allocator.allocateEmplace(Node{key, value, nullptr});
            new_bucket_size = addToBucket(maybe_initial_node.toOpt().value(), new_node);
        }
        else {
            Ref new_node = node_allocator.allocateEmplace(Node{key, value, nullptr});
            buckets.at(keyToBucket(key)) = new_node;
            new_bucket_size = 1;
        }

        load_factor += new_bucket_size * new_bucket_size;
        load_factor -= (new_bucket_size - 1) * (new_bucket_size - 1);

        maybeRehash();
    }

    base::Optional<Ref<T>> atMaybe(const Key& key) {
        auto& bucket = buckets.at(keyToBucket(key));
        for (const auto& node : bucket) {
            if (node.key == key) {
                return node.value;
            }
        }
        return {};
    }

};


template<class Map>
auto testHashMap() {
    Map map;
    for (u64 i = 0; i < 2'000'000; i++) {
        map.put(i, i * 10);
    }
    u64 count = 0;
    for (u64 i = 0; i < 2'000'000; i++) {
        auto it = map.atMaybe(i);
        if (it) {
            count++;
        }
    }
    return count;
}



int main() {
    // auto count = testHashMap<MyHashMap<u64, u64>>();
    auto count = testHashMap<base::StableHashMap<u64, u64>>();
    std::cout << "Number of elements found: " << count << "\n";
    return 0;
}
