#pragma once

#include "node_id_id.hpp"
#include <base/collections/optional.hpp>
#include <base/pointers/ref.hpp>

namespace query::internal {

    template<class Data>
    class NodeIDIDSuperMap final {
        constexpr static u64 BUFFOR_COUNT = 1024 * 4;
        constexpr static u64 BUFFOR_SIZE = 1024 * 1024;


        struct Buffer final {
            std::array<Data, BUFFOR_SIZE> data;
        };

        std::array<std::atomic<Buffer*>, BUFFOR_COUNT> buffers{};

        Ref<Data> getStorageRef(NodeIDID id) {
            u64 internal_id = id.getInternalID();

            u64 buffer_index = internal_id / BUFFOR_SIZE;
            u64 storage_index = internal_id % BUFFOR_SIZE;

            Buffer* buffer = buffers[buffer_index].load(std::memory_order_acquire);
            if (buffer == nullptr) [[unlikely]] {
                // We need to create the buffer. 
                Buffer* expected = nullptr;
                buffer = new Buffer();
                if (!buffers[buffer_index].compare_exchange_strong(expected, buffer, std::memory_order_release, std::memory_order_acquire)) {
                    delete buffer;
                    buffer = expected;
                }
            }

            CORE_ASSERT(buffer != nullptr, "Buffer should have been created");

            return Ref<Data>(buffer->data[storage_index]);
        }

    public:

        Ref<Data> getRef(NodeIDID id) {
            return getStorageRef(id);
        }


        ~NodeIDIDSuperMap() {
            for (auto& buffer : buffers) {
                Buffer* b = buffer.load(std::memory_order_acquire);
                delete b;
            }
        }
    };

}