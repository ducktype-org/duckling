#include "work.hpp"
#include <thread>
#include <iostream>

constexpr int THREAD_COUNT = 4;

int main() {
    HashMap map;
    CHashMap cmap;

    std::vector<std::jthread> threads;
    threads.reserve(THREAD_COUNT);

    for (int i = 0; i < THREAD_COUNT; i++) {
        threads.emplace_back([&]() {
            for (u64 j = 0; j < 40'000'000; j++) {
                if (j % 1'000'000 == 0) {
                    std::cerr << "Thread " << std::this_thread::get_id() << " at iteration " << j << "\n";
                }
                // work(map);
                cWork(cmap);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    return 0;
}