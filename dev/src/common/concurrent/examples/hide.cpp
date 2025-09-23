#include <concurrent/concurrent_allocator.hpp>

concurrent::ConcurrentSingleTypeAllocator<int, 1024*8> allocator;

int* allocateHide() {
    // auto item = allocator.allocateEmplace(42);
    // return &*item;
    return new int(42);
}
