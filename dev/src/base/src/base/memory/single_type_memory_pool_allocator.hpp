#include <base/memory/manual_lifetime_storage.hpp>
#include <base/pointers/box.hpp>
#include <base/misc/noexcept.hpp>
#include <cstddef>
#include <new>

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