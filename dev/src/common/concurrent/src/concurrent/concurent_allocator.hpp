#pragma once

#include <base/ints.hpp>
#include <base/ref.hpp>
#include <base/box.hpp>
#include <optional>
#include <array>
#include "atomic_u64.hpp"

namespace concurrent {

    /**
     * @note: for now it uses the std::optional trick, to avoid 
     * Dealing with object lifetimes.
     * @tparam BLOCK_SIZE -- Number of objects to allocate in one block
     */
    template<class T, u64 BLOCK_SIZE = 1024>
    class ConcurrentSingleTypeAllocator final {
        using IndexT = u64;

        struct Buffer final {
            std::array<std::optional<T>, BLOCK_SIZE> items;
        };

        struct BufferIndex final {
            u64 buffer_idx;
            u64 item_idx;
        };

        constexpr static BufferIndex toBufferIndex(IndexT idx) {
            return BufferIndex{
                .buffer_idx = idx / BLOCK_SIZE,
                .item_idx = idx % BLOCK_SIZE,
            };
        }


    public:

        template<typename... Args>
        Ref<T> allocateEmplace(Args&&... args) {
            // Implementation goes here
        }

        void free(Ref<T> item) {
            // Implementation goes here
        }

    private:
        std::vector<Box<Buffer>> buffers;
        AtomicU64 buffer_count = 0;
        AtomicU64 next_free_idx = 0;

        
    };
} 

