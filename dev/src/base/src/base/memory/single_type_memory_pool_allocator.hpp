#include <base/memory/manual_lifetime_storage.hpp>
#include <base/pointers/box.hpp>

namespace base {

    /**
     * A memory pool object of a single type allocator.
     * It manages objects, not memory.
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
		};
    
    public:
        SingleTypeMemoryPoolAllocator() = default;
        SingleTypeMemoryPoolAllocator(const SingleTypeMemoryPoolAllocator&) = delete;
        SingleTypeMemoryPoolAllocator(SingleTypeMemoryPoolAllocator&&) = default;

        SingleTypeMemoryPoolAllocator& operator=(const SingleTypeMemoryPoolAllocator&) = delete;
        SingleTypeMemoryPoolAllocator& operator=(SingleTypeMemoryPoolAllocator&&) = default;


        Ref<T> allocateEmplace(auto&&... args) {
			
		}

        void deallocate(Ref<T> obj_ref) {

        }
        
        ~SingleTypeMemoryPoolAllocator() {
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