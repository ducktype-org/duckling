#include <base/collections/maps.hpp>
#include <base/pointers/shared_box.hpp>
#include <base/types/ints.hpp>

#include <vm/core/process/concurrency/object_pool.hpp>

#include <mutex>

namespace vm {
	class SynchronizationPrimitives final {
	private:
		/**
		 * @brief Pool for mutexes used in the process.
		 */
		ObjectPool<std::mutex> mutex_pool;

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
	};
}
