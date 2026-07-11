#pragma once

#include "condition_variable.hpp"

#include <base/collections/maps.hpp>
#include <base/collections/object_pool.hpp>
#include <base/collections/optional.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <vm/core/process/concurrency/fast_track/vc.hpp>

#include <mutex>

namespace vm {
	/**
	 * @brief A DVM mutex: the OS mutex plus, when the process runs with Fast Track, the vector
	 * clock the lock carries between its releaser and the next acquirer.
	 */
	struct Mutex final {
		std::timed_mutex            os_mutex;
		base::Optional<VectorClock> vc;
	};

	class SynchronizationPrimitives final {
	private:
		/**
		 * @brief Pool for mutexes used in the process.
		 */
		base::StableObjectPool<Mutex, u64, false> mutex_pool;

		/**
		 * @brief Pool for condition variables used in the process.
		 */
		base::StableObjectPool<ConditionVariable, u64, false> cv_pool;


	public:
		~SynchronizationPrimitives();

		/**
		 * @brief Getter for mutexes in the pool.
		 */
		Ref<Mutex> getMutex(usize mutex_id);

		/**
		 * @brief Adds new mutex into pool.
		 */
		usize addMutex();

		/**
		 * @brief Removes mutex from pool.
		 */
		void removeMutex(usize);


		/**
		 * @brief Getter for condition variables in the pool.
		 */
		base::Ref<ConditionVariable> getCV(usize cv_id);

		/**
		 * @brief Adds new condition variable into pool.
		 */
		usize addCV();

		/**
		 * @brief Removes condition variable from pool.
		 */
		void removeCV(usize);
	};
}
