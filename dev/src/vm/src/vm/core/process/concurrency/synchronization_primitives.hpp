#include <base/collections/maps.hpp>
#include <base/collections/object_pool.hpp>
#include <base/pointers/shared_box.hpp>
#include <base/types/ints.hpp>

#include <mutex>

namespace vm {
	class SynchronizationPrimitives final {
	private:
		/**
		 * @brief Pool for mutexes used in the process.
		 */
		base::ObjectPool<std::mutex> mutex_pool;

	public:
		/**
		 * @brief Getter for mutexes in the pool.
		 */
		Ref<std::mutex> getMutex(i64 mutex_id);

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
