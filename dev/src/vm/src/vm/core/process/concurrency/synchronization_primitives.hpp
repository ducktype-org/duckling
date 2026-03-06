#include <base/collections/maps.hpp>
#include <base/pointers/shared_box.hpp>
#include <base/types/ints.hpp>
#include "condition_variable.hpp"

#include <vm/core/process/concurrency/object_pool.hpp>

#include <mutex>

namespace vm {
	class SynchronizationPrimitives final {
	private:
		/**
		 * @brief Pool for mutexes used in the process.
		 */
		ObjectPool<std::mutex> mutex_pool;


		i64 next_cv_id = 0;

		/**
		 * @brief Pool for condition variables used in the process.
		 */
		ObjectPool<ConditionVariable> cv_pool;


	public:
		/**
		 * @brief Getter for mutexes in the pool.
		 */
		SharedBox<std::mutex> getMutex(i64 mutex_id);

		/**
		 * @brief Adds new mutex into pool.
		 */
		i64 addMutex();

		/**
		 * @brief Removes mutex from pool.
		 */
		void removeMutex(i64);


		/**
		 * @brief Getter for condition variables in the pool.
		 */
		SharedBox<ConditionVariable> getCV(i64 cv_id);
		
		/**
		 * @brief Adds new condition variable into pool.
		 */
		i64 addCV();
		
		/**
		 * @brief Removes condition variable from pool.
		 */
		void removeCV(i64);

	};
}
