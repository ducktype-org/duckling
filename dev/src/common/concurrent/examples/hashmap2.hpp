#pragma once

#include <base/collections/maps.hpp>
#include <base/pointers/box.hpp>

#include <iterator>
#include <type_traits>
#include <utility>

#include <base/pointers/box.hpp>
#include <base/misc/noexcept.hpp>
#include <cstddef>
#include <new>

#include <utility>
#include <base/comptime/type_traits.hpp>
#include <base/config/build_type.hpp>
#include <base/misc/noexcept.hpp>
#include <base/pointers/ref.hpp>


namespace base {

	/**
	 * Explicit lifetime management for a single object.
     *
     * It is not aware of any of the object semantics, it just stores see raw bytes and allows to
     * construct and destroy the object in place.
     *
	 * Any wrong usage results in undefined behavior.
	 * See also: https://en.cppreference.com/w/cpp/utility/launder.html
	 */
	template<class T>
    requires base::IsPlainType<T>
	struct ManualLifetimeStorage final {
	private:
		alignas(T) std::byte data[sizeof(T)] = {}; // NOLINT

        enum class State { Empty, Constructed };
        IF_BUILD_TYPE_DEV(State state = State::Empty;)

	public:
        ManualLifetimeStorage() = default;

        ManualLifetimeStorage(const ManualLifetimeStorage&) = delete;
        ManualLifetimeStorage(ManualLifetimeStorage&&) = delete;
        ManualLifetimeStorage& operator=(const ManualLifetimeStorage&) = delete;
        ManualLifetimeStorage& operator=(ManualLifetimeStorage&&) = delete;

        // We set it explicitly, to make sure that in release builds destructor is trivial:
        IF_BUILD_TYPE_RELEASE(
            ~ManualLifetimeStorage() noexcept = default;
        )
        IF_BUILD_TYPE_DEV(
            ~ManualLifetimeStorage() RELEASE_NOEXCEPT {
                CORE_ASSERT(state == State::Empty, "Object is still constructed during destruction of ManualLifetimeStorage");
            }
        )


		template<class... Args>
		void construct(Args&&... args) {
            IF_BUILD_TYPE_DEV({
                CORE_ASSERT(state == State::Empty, "Object is already constructed");
                state = State::Constructed;
            })
			new (data) T(std::forward<Args>(args)...);
		}

		void destroy() {
            IF_BUILD_TYPE_DEV({
                CORE_ASSERT(state == State::Constructed, "Object is not constructed (destroy)");
            })
            this->get()->~T();
            // this has to be after destruction, so we can also assert in get():
            IF_BUILD_TYPE_DEV({
                state = State::Empty;
            })
        }

		T* get() { 
            IF_BUILD_TYPE_DEV({
                CORE_ASSERT(state == State::Constructed, "Object is not constructed (get)");
            })
            return std::launder(reinterpret_cast<T*>(&data));
        }

        /**
         * Obtains pointer to the ManualLifetimeStorage from the object reference.
         * @note Behavior is undefined if obj_ref was not constructed in ManualLifetimeStorage.
         * Use with caution.
         * 
         * @note offsetof is conditionally supported for non-standard-layout types since C++17.
         * If this breaks, figure it out. 
         *
         * @note I'm not 100% sure the function is well defined here.
         */
        static Ref<ManualLifetimeStorage> getSelf(Ref<T> obj_ref) {
            constexpr auto OFFSET = offsetof(ManualLifetimeStorage, data);
            static_assert(OFFSET == 0, "This might not hold actually, but should. Is left here for clarity, and with it I'm more confident this is UB free.");

            return std::launder(reinterpret_cast<ManualLifetimeStorage*>(
                reinterpret_cast<std::byte*>(obj_ref.get()) - OFFSET
            ));
        }
	};

}

namespace base {

    /**
     * A memory pool object of a single type allocator.
     * It manages objects, not memory.
     *
     * @note After some IRL debates it was concluded that allocate/deallocate api should
     * work on plain Refs, and deallocation must internally search for a buffer and an index of the object.
     * This might be less time efficient, but it greatly simplifies the api and
     * lowers memory usage and can be made in logarithmic time if needed in the future.
     */
    template<class T, u64 BLOCK_SIZE = 2'048>
    requires base::IsPlainType<T>
	class SingleTypeMemoryPoolAllocator final {
    private:
        using StorageT = ManualLifetimeStorage<T>;

        /**
         * A single buffer (i.e. "pool") of objects.
         */
        struct Buffer final {
			std::array<StorageT, BLOCK_SIZE> items;
		};

    
        /**
         * Simple helper type that wraps index of a buffer and index of an item within that buffer.
         */
        struct BufferIndex final {
			u64 buffer_idx = 0;
			u64 item_idx   = 0;

            BufferIndex next() const {
                BufferIndex next_idx = *this;
                next_idx.item_idx++;
                if (next_idx.item_idx >= BLOCK_SIZE) {
                    next_idx.buffer_idx++;
                    next_idx.item_idx = 0;
                }
                return next_idx;
            }
		};
    
    public:
        SingleTypeMemoryPoolAllocator() = default;
        SingleTypeMemoryPoolAllocator(const SingleTypeMemoryPoolAllocator&) = delete;
        SingleTypeMemoryPoolAllocator(SingleTypeMemoryPoolAllocator&&) = default;

        SingleTypeMemoryPoolAllocator& operator=(const SingleTypeMemoryPoolAllocator&) = delete;
        SingleTypeMemoryPoolAllocator& operator=(SingleTypeMemoryPoolAllocator&&) = default;


        /**
         * Allocates a new object in the pool and constructs it with the given arguments.
         */
        Ref<T> allocateEmplace(auto&&... args) {
            allocated_count++;

            // idx, in which we will allocate the new object:
            BufferIndex allocation_idx;

            if (not free_list.empty()) {
                allocation_idx = free_list.back();
                free_list.pop_back();
            }
            else {    
                // else allocate in the next available slot:
                allocation_idx = next_buffer_idx;
                next_buffer_idx = next_buffer_idx.next();

                // check if we need to allocate a new buffer:
                if (allocation_idx.buffer_idx >= buffers.size()) [[unlikely]] {
                    buffers.emplace_back(makeBox<Buffer>());
                }
            }

            CORE_ASSERT(allocation_idx.buffer_idx < buffers.size(), "Buffer index out of bounds");
            CORE_ASSERT(allocation_idx.item_idx < BLOCK_SIZE, "Item index out of bounds");

            Ref<StorageT> new_storage = &buffers[allocation_idx.buffer_idx]->items[allocation_idx.item_idx];
            new_storage->construct(std::forward<decltype(args)>(args)...);
            return new_storage->get();
		}

        /**
         * Destroys the given object without deallocating its memory.
         * The allocator will treat the memory as still allocated.
         * @note If object pointed to by obj_ref was not allocated by this allocator,
         * behavior is undefined, EVEN IN DEV BUILDS.
         */
        void justDestroy(Ref<T> obj_ref) {
            allocated_count--;
            Ref<StorageT> obj_storage = StorageT::getSelf(obj_ref);
            obj_storage->destroy();
        }

        /**
         * Deallocates and destroys the given object.
         * @note If object pointed to by obj_ref was not allocated by this allocator,
         * behavior is undefined, EVEN IN DEV BUILDS.
         */
        void deallocateDestroy(Ref<T> obj_ref) {
            allocated_count--;

            Ref<StorageT> obj_storage = StorageT::getSelf(obj_ref);

            // idx, in which the object is stored:
            BufferIndex deallocation_idx;

            bool found = false;

            // naively find the buffer and the index within the buffer:
            for (u64 buffer_idx = 0; buffer_idx < buffers.size(); buffer_idx++) {
                StorageT* buffer_pointer_start = buffers[buffer_idx]->items.data();
                StorageT* buffer_pointer_end = buffers[buffer_idx]->items.data() + buffers[buffer_idx]->items.size();
                if (buffer_pointer_start <= obj_storage.get() and obj_storage.get() < buffer_pointer_end) {
                    deallocation_idx.buffer_idx = buffer_idx;
                    deallocation_idx.item_idx = static_cast<u64>(obj_storage.get() - buffer_pointer_start);
                    found = true;
                    break;
                }
            }

            CORE_ASSERT(found, "Object to deallocate was not allocated by this allocator");

            // actually destroy and deallocate the object:
            obj_storage->destroy();
            free_list.push_back(deallocation_idx);
        }
        
        ~SingleTypeMemoryPoolAllocator() RELEASE_NOEXCEPT {
            CORE_ASSERT(allocated_count == 0, "Not all allocated objects were deallocated before destruction of the allocator");
        }
    private:
        /**
         * Actual storage for all allocated buffers.
         * They are boxed, so that they are not moved in memory when the vector resizes.
         */
        std::vector<Box<Buffer>> buffers;

        /**
         * List of free items in the pools.
         */
        std::vector<BufferIndex> free_list;

        /**
         * Total number of allocated items in the pool.
         */
        u64 allocated_count = 0;

        BufferIndex next_buffer_idx = {0, 0};


    };
}


namespace base {
    	template<typename KEY_T, typename DATA_T, typename HASH_T = std::hash<KEY_T>>
	class StableHashMap20 final {

	public:
		struct KeyValuePair final {
			const KEY_T key;
			DATA_T     value;

			KeyValuePair(const KEY_T& key, const DATA_T& value) noexcept
				: key(key), value(value) {}
			
			KeyValuePair(KEY_T&& key, DATA_T&& value) noexcept
				: key(std::move(key)), value(std::move(value)) {}
		};

	private:

		static constexpr usize  INITIAL_BUCKETS = 16;
		static constexpr double MAX_LOAD_FACTOR = 0.7;

		using KeyHash = u64;

		struct Node final {
			MRef<Node> next;

			KeyValuePair key_value;

			Node(MRef<Node> next, const KEY_T& key, const DATA_T& value) noexcept
				: next(next), key_value(key, value) {}

			Node(MRef<Node> next, KEY_T&& key, DATA_T&& value) noexcept
				: next(next), key_value(std::move(key), std::move(value)) {}
		};

		std::vector<MRef<Node>>              buckets;
		SingleTypeMemoryPoolAllocator<Node, 64>  node_allocator;
		u64                                  element_count = 0;

		[[nodiscard]]
		static auto keyHash(const KEY_T& key) {
			return HASH_T{}(key);
		}

		[[nodiscard]]
		u64 keyToBucket(const KEY_T& key) const RELEASE_NOEXCEPT {
			u64 hash = keyHash(key);
			auto res = hash % buckets.size();
			CORE_ASSERT(0 <= res and res < buckets.size(), "Bucket index out of bounds");
			return res;
		}

		void rehash() RELEASE_NOEXCEPT {
			usize                  new_bucket_count = buckets.size() * 2;
			
			std::vector<Ref<Node>> all_nodes;
			all_nodes.reserve(element_count);

			for (const auto& bucket: buckets) {
				MRef<Node> current_node = bucket;
				while (current_node) {
					all_nodes.emplace_back(current_node.toOpt().value());
					current_node = current_node.toOpt().value()->next;
					all_nodes.back()->next = nullptr;
				}
			}

			CORE_ASSERT(all_nodes.size() == element_count, "Node count mismatch during rehash: ", all_nodes.size(), " vs ", element_count);

			buckets.clear();
			buckets.resize(new_bucket_count);

			for (const Ref<Node>& node: all_nodes) {
				addToBucket(keyToBucket(node->key_value.key), node);
			}
		}

		/**
		* Appends new node to the bucket identified by node reference.
		* Panics if node with the same key already exists.
		*/
		void addToBucketNode(Ref<Node> bucket, Ref<Node> new_node) RELEASE_NOEXCEPT {
			CORE_ASSERT(new_node->next == nullptr, "New node must be ending node");

			Ref current_node = bucket;
			const KEY_T& key = new_node->key_value.key;

			while (true) {
				if (current_node->key_value.key == key) {
					// this can be changed to an assertion:
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

		void addToBucket(u64 bucket_index, Ref<Node> new_node) RELEASE_NOEXCEPT {
			MRef maybe_initial_node = buckets.at(bucket_index);
			if (maybe_initial_node)
				addToBucketNode(maybe_initial_node.toOpt().value(), new_node);
			else
				buckets.at(bucket_index) = new_node;
		}

		void maybeRehash() RELEASE_NOEXCEPT {
			if (double(element_count) > MAX_LOAD_FACTOR * double(buckets.size())) rehash();
		}

	public:
		StableHashMap20(): buckets(INITIAL_BUCKETS) {}
		StableHashMap20(const StableHashMap20&) = delete;
		StableHashMap20(StableHashMap20&&) = default;

		~StableHashMap20() {
			for (auto& bucket: buckets) {
				MRef<Node> current_node = bucket;
				while (current_node) {
					MRef<Node> next_node = current_node->next;
					node_allocator.justDestroy(current_node.toOpt().value());
					current_node = next_node;
				}
			}
		};

		/**
		 * This is a template, so we can can have const and non-const versions.
		 */
		template<class ValueT>
		class Iterator final {
			// end is represented by bucket_index == buckets.size(), and current_node == nullptr

			u64 bucket_index = 0;

			// mref, since we have to represent "end" iterator.
			MRef<Node> current_node;

			MRef<Node>* buckets_array_ptr;

			u64 buckets_size;

		public:
			using iterator_category = std::forward_iterator_tag;
			using difference_type   = u64;
			using value_type        = ValueT;
			using pointer           = value_type*;
			using reference         = value_type&;

			Iterator(u64 bucket_index, MRef<Node> current_node, MRef<Node>* buckets_array_ptr, u64 buckets_size) noexcept
				: bucket_index(bucket_index), current_node(current_node), buckets_array_ptr(buckets_array_ptr), buckets_size(buckets_size) {}

			Iterator(const Iterator&) = default;
			Iterator(Iterator&&) = default;
			Iterator& operator=(const Iterator&) = default;
			Iterator& operator=(Iterator&&) = default;

			reference operator*() const {
				return current_node->key_value;
			}
			pointer operator->() const {
				return &current_node->key_value;
			}

			Iterator& operator++() {
				if (current_node->next != nullptr) {
					current_node = current_node->next.toOpt().value();
				}
				else {
					bucket_index++;
					current_node = nullptr;

					while (bucket_index < buckets_size and buckets_array_ptr[bucket_index] == nullptr) {
						bucket_index++;
					}

					if (bucket_index < buckets_size) {
						current_node = buckets_array_ptr[bucket_index].toOpt().value();
					}
				}
				return *this;
			}  

			friend bool operator== (const Iterator& a, const Iterator& b) { 
				return std::tie(a.bucket_index, a.current_node, a.buckets_array_ptr, a.buckets_size) ==
				 std::tie(b.bucket_index, b.current_node, b.buckets_array_ptr, b.buckets_size);
			};
			friend bool operator!= (const Iterator& a, const Iterator& b) { 
				return !(a == b);
			};

		};

		using IteratorT = Iterator<KeyValuePair>;
		using ConstIteratorT = Iterator<const KeyValuePair>;




		void put(const KEY_T& key, const DATA_T& value) RELEASE_NOEXCEPT {
			auto new_node
				= node_allocator.allocateEmplace(Node{ nullptr, key, value });

			addToBucket(keyToBucket(key), new_node);

			element_count++;
			maybeRehash();
		}

		[[nodiscard]]
		base::Optional<Ref<DATA_T>> atMaybe(const KEY_T& key) const RELEASE_NOEXCEPT {
			auto current_node = buckets.at(keyToBucket(key));
			while (current_node) {
				if (current_node->key_value.key == key) return &current_node->key_value.value;
				current_node = current_node->next;
			}
			return {};
		}

		[[nodiscard]]
		bool contains(const KEY_T& key) const RELEASE_NOEXCEPT {
			return atMaybe(key).has_value();
		}

		IteratorT begin() RELEASE_NOEXCEPT {
			u64 bucket_index = 0;
			MRef<Node> current_node = nullptr;

			while (bucket_index < buckets.size()) {
				current_node = buckets[bucket_index];
				if (current_node != nullptr) break;
				bucket_index++;
			}

			return IteratorT(bucket_index, current_node, buckets.data(), buckets.size());	
		}

		IteratorT end() RELEASE_NOEXCEPT {
			return IteratorT(buckets.size(), nullptr, buckets.data(), buckets.size());
		}

		ConstIteratorT begin() const RELEASE_NOEXCEPT {
			u64 bucket_index = 0;
			MRef<Node> current_node = nullptr;

			while (bucket_index < buckets.size()) {
				current_node = buckets[bucket_index];
				if (current_node != nullptr) break;
				bucket_index++;
			}

			return ConstIteratorT(bucket_index, current_node.toOpt().copyValueOr(nullptr), buckets.data(), buckets.size());	
		}

		ConstIteratorT end() const RELEASE_NOEXCEPT {
			return ConstIteratorT(buckets.size(), nullptr, buckets.data(), buckets.size());
		}
	};
} 