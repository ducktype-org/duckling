#include "hide.hpp"

#include <concurrent/concurrent_allocator.hpp>

concurrent::ConcurrentSingleTypeAllocator<THide, 1'024 * 8> allocator;

THide* allocateHide() {
	auto item = allocator.allocateEmplace(42);
	return &*item;
	// return new THide(42);
}
