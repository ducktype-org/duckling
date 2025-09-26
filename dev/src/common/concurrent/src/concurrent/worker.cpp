#include "worker.hpp"

namespace concurrent {
	WorkerData::WorkerData(u64 id, std::minstd_rand rng): id(id), rng(rng) {}

	WorkerData WorkerData::make() {
		static u64 next_id = 0;
		return { next_id++, std::minstd_rand{ std::random_device{}() } };
	}
}
