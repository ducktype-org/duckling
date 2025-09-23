#include "hide.hpp"

#include <thread>
#include <vector>

void job() {
	for (int i = 0; i < 10'000'000; i++) allocateHide();
}


int main() {
	// spawn NUM_THREADS threads doing allocations:
	std::vector<std::thread> threads;
	threads.reserve(NUM_THREADS);
	for (int i = 0; i < NUM_THREADS; i++) threads.emplace_back(job);
	for (auto& t: threads) t.join();
	return 0;
}
