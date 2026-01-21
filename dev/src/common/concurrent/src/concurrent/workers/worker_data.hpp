#pragma once

#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <random>

namespace concurrent {

	struct WorkerData;

	/**
	 * Utility alias for a reference to WorkerData.
	 */
	using WDRef = Ref<WorkerData>;

	/**
	 * Unique data associated with each worker.
	 * This data can be freely used by each worker without additional synchronization.
	 * @important Each data can be accessed only by the worker it belongs to.
	 *
	 * See the README of this module for more information about workers.
	 */
	struct WorkerData final {
	private:
		WorkerData(u64 id, std::mt19937_64 rng);

		u64             id;
		std::mt19937_64 rng;

		/**
		 * Internall method to create WorkerData instances.
		 */
		static CRef<std::vector<WDRef>> generateWorkerData();

	public:
		WorkerData()                  = delete;
		WorkerData(const WorkerData&) = delete;
		WorkerData(WorkerData&&)      = delete;

		WorkerData& operator=(const WorkerData&) = delete;
		WorkerData& operator=(WorkerData&&)      = delete;

		[[nodiscard]]
		u64 getID() const {
			return id;
		}

		u64 randomU64() { return u64(rng()); }

		/**
		 * Get the worker data for all workers.
		 * Can be called multiple times.
		 * Can be called concurrently from multiple threads.
		 *
		 * @note It is usually better to retrieve the worker data reference once
		 *       and store it in a thread-local variable for fast access instead of calling this
		 * method repeatedly.
		 */
		static CRef<std::vector<WDRef>> getWorkerData();
	};


}
