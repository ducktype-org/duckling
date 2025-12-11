#pragma once

#include <base/types/ints.hpp>
#include <base/pointers/ref.hpp>
#include <base/pointers/box.hpp>
#include <random>

namespace concurrent {

	struct WorkerData;
	using WDRef = Ref<WorkerData>;

	/**
	 * Each worker -- i.e. thread -- has its own global worker-local data.
	 * This data can then be used in various places allowing for more efficient
	 * implementations.
	 */
	struct WorkerData final {
	private:
		WorkerData(u64 id, std::mt19937_64 rng);

		u64 id;
		std::mt19937_64 rng;

		/** 
		 * Internall method to create WorkerData instances.
		 */
		static CRef<std::vector<WDRef>> generateWorkerData();

	public:
		WorkerData()                              = delete;
		WorkerData(const WorkerData&)             = delete;
		WorkerData(WorkerData&&)                  = delete;

		WorkerData& operator=(const WorkerData&)  = delete;
		WorkerData&       operator=(WorkerData&&) = delete;

		[[nodiscard]]
		u64 getID() const { return id; }

		u64 randomU64() {
			return u64(rng());
		}

		
		/** 
		 * Get the worker data for all workers.
		 * Can be called multiple times.
		 * Can be called concurrently from multiple threads.
		 *
		 * @note It is usually better to retrieve the worker data reference once
		 *       and store it in a thread-local variable for fast access instead of calling this method repeatedly.
		 */
		static CRef<std::vector<WDRef>> getWorkerData();
	};

	

}
