#include <concurrent/concurrent_allocator.hpp>

concurrent::ConcurrentSingleTypeAllocator<int> allocator;

int* allocateHide() {
    auto item = allocator.allocateEmplace(42);
    return &*item;
}
