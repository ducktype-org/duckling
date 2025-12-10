#include "work.hpp"
#include <thread>
#include <iostream>
#include <concurrent/workers/worker_data.hpp>

constexpr int THREAD_COUNT = 8;

// u64 concurrent::AtomicFlagSpinlock::yield_count = 0;



int main() {
    HashMap map;
    CHashMap cmap;

    std::vector<std::jthread> threads;
    std::vector<Box<concurrent::WorkerData>> wd;
    
    threads.reserve(THREAD_COUNT);
    wd.reserve(THREAD_COUNT);

    concurrent::setWorkerCount(THREAD_COUNT);

    for (int i = 0; i < THREAD_COUNT; i++) {
        wd.emplace_back(concurrent::WorkerData::make());
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        

        threads.emplace_back([i, &map, &cmap, &wd]() mutable {
            (void)map;
            (void)cmap;

            for (u64 j = 0; j < 40'000'000; j++) {
                if (j % 1'000'000 == 0) {
                    std::cerr << "Thread " << std::this_thread::get_id() << " at iteration " << j  << "\n";
                }
                // work(map,  wd.at(u64(i)).refMut()); static_assert(THREAD_COUNT == 1);
                cWork(cmap, wd.at(u64(i)).refMut());
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }


    return 0;
}