#include "hide.hpp"

#include <concurrent/concurrent_allocator.hpp>
#include <concurrent/concurrent_allocator_take_2.hpp>

// concurrent::ConcurrentSingleTypeAllocator<THide, 1'024 * 8> allocator;
//concurrent::SingleThreadedAllocator<THide, 1'024ull * 128> allocator;
concurrent::ConcurrentSingleTypeAllocatorTake2<THide, 1'024 * 8, NUM_THREADS> allocator;

THide* allocateHide(Ref<concurrent::WorkerData> worker) {
	// auto item = allocator.allocateEmplace(worker, 42);
	// return &*item;
	return new THide(42);
}

// void deallocateHide(THide* item) {
// 	// allocator.deallocate(item);
// 	delete item;
// }
