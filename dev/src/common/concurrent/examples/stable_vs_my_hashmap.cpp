#include <concurrent/concurrent_allocator_take_2.hpp>

#include <base/maps.hpp>
#include <base/stable_hashmap.hpp>
#include <base/noexcept.hpp>

#include <iostream>

template<class K, class T>
class MyHashMap {
	// @EH: this still perform alloc per element (argh!)
	base::HashMap<K, Ref<T>>                      map;
	concurrent::SingleThreadedAllocator<T, 2'048> allocator;

public:
	MyHashMap() = default;

	void put(const K& key, const T& value) {
		auto item = allocator.allocateEmplace(value);
		map.put(key, item);
	}

	base::Optional<Ref<T>> atMaybe(const K& key) {
		auto maybe_ref = map.atMaybe(key);
		if (maybe_ref)
			return *maybe_ref.value();
		else
			return {};
	}
};

// @TODO: it is bugged!
// assume u64 keys
template<class T>
class MyCustomHashMap final {
	static constexpr usize  INITIAL_BUCKETS = 256;
	static constexpr double MAX_LOAD_FACTOR = 0.6;

	using Key = u64;

	struct Node final {
		MRef<Node> next;
		Key        key;
		T          value;
		
		Node(MRef<Node> next, Key key, T value) noexcept
			: next(next), key(key), value(value) {}
	};

	std::vector<MRef<Node>>                          buckets;
	concurrent::SingleThreadedAllocator<Node, 1024>  node_allocator;
	u64                                              element_count = 0;

	[[nodiscard]]
	u64 keyToBucket(const Key& key) const NOEXCEPT {
		return key % buckets.size();
	}

	void rehash() NOEXCEPT{
		usize                  new_bucket_count = buckets.size() * 4;
		
		std::vector<Ref<Node>> all_nodes;
		all_nodes.reserve(element_count);

		for (const auto& bucket: buckets) {
			auto current_node = bucket;
			while (current_node) {
				all_nodes.emplace_back(current_node.toOpt().value());
				all_nodes.back()->next = nullptr;

				current_node = current_node.toOpt().value()->next;
			}
		}

		buckets.clear();
		buckets.resize(new_bucket_count);

		// @opt: keep bucket ends -- pointers to nullptr MRef nexts
		// can be used here and in addToBucket to avoid traversing the whole bucket
		// when adding new nodes

		for (const auto& node: all_nodes) {
			auto bucket = buckets[keyToBucket(node->key)];
			if (bucket)
				addToBucket(bucket.toOpt().value(), node);
			else
				buckets[keyToBucket(node->key)] = node;
		}
	}

	/**
	 * Appends new node to the bucket identified by node reference.
	 * Returns new size of the bucket.
	 * Checks if a node with the same key already exists - if so, throws logic_error.
	 */
	void addToBucket(Ref<Node> bucket, Ref<Node> new_node) NOEXCEPT {
		Ref current_node = bucket;
		Key key          = new_node->key;

		while (true) {
			if (current_node->key == key) {
				// this can be changed to an assertion:
				// throw std::logic_error("Duplicate key insertion in MyCustomHashMap");
				CORE_PANIC("Duplicate key insertion in MyCustomHashMap");
			}
			if (current_node->next)
				current_node = current_node->next.toOpt().value();
			else
				break;
		}
		CORE_ASSERT(current_node->next == nullptr, "this must be the last node in the bucket");
		current_node->next = new_node;
	}

	void maybeRehash() NOEXCEPT {
		if (double(element_count) > MAX_LOAD_FACTOR * double(buckets.size())) rehash();
	}

public:
	MyCustomHashMap(): buckets(INITIAL_BUCKETS) {}

	void put(const Key& key, const T& value) NOEXCEPT {
		MRef maybe_initial_node = buckets[keyToBucket(key)];
		auto new_node
			= node_allocator.allocateEmplace(Node{ nullptr, key, value });

		if (maybe_initial_node)
			addToBucket(maybe_initial_node.toOpt().value(), new_node);
		else
			buckets.at(keyToBucket(key)) = new_node;

		element_count++;
		maybeRehash();
	}

	[[nodiscard]]
	base::Optional<Ref<T>> atMaybe(const Key& key) const NOEXCEPT {
		auto current_node = buckets.at(keyToBucket(key));
		while (current_node) {
			if (current_node->key == key) return &current_node->value;
			current_node = current_node->next;
		}
		return {};
	}
};

std::minstd_rand rng(42);

template<class Map>
auto testHashMap() {
	constexpr static usize  NUM_ELEMENTS = 2'000'000;

	Map map;
	for (u64 i = 0; i < NUM_ELEMENTS; i++) {
		map.put(rng()%NUM_ELEMENTS, rng());
	}
	u64 count = 0;
	for (u64 i = 0; i < NUM_ELEMENTS; i++) {
		auto it = map.atMaybe(rng()%NUM_ELEMENTS);
		if (it) count++;
	}
	return count;
}

int main() {
	// auto count = testHashMap<base::StableHashMap<u64, u64>>();
	// auto count = testHashMap<MyHashMap<u64, u64>>();
	auto count = testHashMap<MyCustomHashMap<u64>>();
	std::cout << "Number of elements found: " << count << "\n";
	return 0;
}
