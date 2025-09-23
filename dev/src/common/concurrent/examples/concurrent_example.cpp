#include "hide.hpp"

#include <thread>
#include <vector>

void job() {
	for (int i = 0; i < 1'000'000; i++) allocateHide();
}

int main() {
	// spawn 8 threads doing allocations:
	std::vector<std::thread> threads;
	threads.reserve(8);
	for (int i = 0; i < 8; i++) threads.emplace_back(job);
	for (auto& t: threads) t.join();
	return 0;
}
