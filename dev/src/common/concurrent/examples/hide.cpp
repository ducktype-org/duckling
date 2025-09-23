#include "hide.hpp"

#include <concurrent/concurrent_allocator.hpp>
#include <concurrent/concurrent_allocator_take_2.hpp>

// concurrent::ConcurrentSingleTypeAllocator<THide, 1'024 * 8> allocator;
//concurrent::SingleThreadedAllocator<THide, 1'024ull * 128> allocator;

// IMPORTANT NOTE: 2048 is best for performance with THide beeing 4x u64
// it has an impact of about 3x performance over bad sizes
// we have to build some kind of benchmarking tool/heuristic to find the best size automatically
concurrent::ConcurrentSingleTypeAllocatorTake2<THide, 2048, NUM_THREADS> allocator;

// std::deque<THide> allocated_items;

THide* allocateHide(Ref<concurrent::WorkerData> worker) {
	auto item = allocator.allocateEmplace(worker, 42);
	return &*item;


	// return new THide(42);

	// allocated_items.emplace_back();
	// return &allocated_items.back();
}

// void deallocateHide(THide* item) {
// 	// allocator.deallocate(item);
// 	delete item;
// }
