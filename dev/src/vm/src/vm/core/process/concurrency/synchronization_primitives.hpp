#include <base/collections/maps.hpp>
#include <base/pointers/shared_box.hpp>
#include <base/types/ints.hpp>

#include <vm/core/process/concurrency/object_pool.hpp>

#include <mutex>
#include <condition_variable>

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
		base::HashMap<i64, SharedBox<std::condition_variable_any>>
			cv_map;  // @TODO: #2109 Find better structure then map for storing condition variables.


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


		SharedBox<std::condition_variable_any> getCV(i64 cv_id);
		i64 addCV();
		void removeCV(i64);

	};
}
