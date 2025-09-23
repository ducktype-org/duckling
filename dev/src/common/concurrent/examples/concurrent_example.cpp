#include "hide.hpp"

#include <thread>
#include <vector>
#include <iostream>

#include <concurrent/manual_object_storage.hpp>

void job() {
	for (int i = 0; i < 10'000'000; i++) allocateHide();
}

// class A {
// public:	
// 	int x = 0x01;
// 	virtual ~A() = default;
// };


template<class T>
void printBytes(const T& obj) {
	auto bytes = std::as_bytes(std::span<const T>(&obj, 1));
	for (auto b: bytes) {
		std::cerr << std::hex << (int)b << " ";
	}
	std::cerr << "\n";
}


int main() {
	// spawn NUM_THREADS threads doing allocations:
	std::vector<std::thread> threads;
	threads.reserve(NUM_THREADS);
	for (int i = 0; i < NUM_THREADS; i++) threads.emplace_back(job);
	for (auto& t: threads) t.join();
	return 0;

	// concurrent::ObjStorage<int> storage;
	// storage.construct(42);
	// std::cerr << *storage.get() << "\n";
	// storage.destroy();

	// concurrent::ObjStorage<A> storageA;
	// storageA.construct();
	// storageA.get();

	// // std::cerr << &*storageA.get() << "\n";
	// // std::cerr << &storageA << "\n";
	// // std::cerr << std::as_bytes()

	// printBytes(storageA);

	// storageA.destroy();

	// A a;
	// // printBytes(a);
	// std::cerr << &a << "\n";
	// std::cerr << &(a.x) << "\n";


}
