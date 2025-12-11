#include "work.hpp"

#include <concurrent/workers/worker_data.hpp>

#include <random>

// #include <mutex>


void work(HashMap& map, concurrent::WDRef worker_data) {
	u64 key = 123'123 + (worker_data->rng() % 1'024);

	map.maybePut(key, 0);
	auto new_val = map[key] + 1;
	map[key]     = new_val;
}

void cWork(CHashMap& map, concurrent::WDRef worker_data) {
	u64 key = 123'123 + (worker_data->rng() % 12);


	// map.tryPut(key, 0);
	// auto new_val = map.getCopy(key) + 1;
	// map.update(key, new_val);

	// single atomic operation:
	map.tryPutAndUpdate(key, 0, [](u64& val) { val = val + 1; });
}
