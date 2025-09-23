#include "hide.hpp"

#include <concurrent/concurrent_allocator.hpp>
#include <concurrent/concurrent_allocator_take_2.hpp>

// concurrent::ConcurrentSingleTypeAllocator<THide, 1'024 * 8> allocator;
concurrent::SingleThreadedAllocator<THide, 1'024ull * 128> allocator;

THide* allocateHide() {
	auto item = allocator.allocateEmplace(42);
	return &*item;
	// return new THide(42);
}
