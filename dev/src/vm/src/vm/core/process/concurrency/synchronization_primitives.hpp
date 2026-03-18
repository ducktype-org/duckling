#include <base/collections/stable_hashmap.hpp>
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
		base::StableObjectPool<std::mutex, u64, false> mutex_pool;

	public:
		/**
		 * @brief Getter for mutexes in the pool.
		 */
		Ref<std::mutex> getMutex(usize mutex_id);

		/**
		 * @brief Adds new mutex into pool.
		 */
		usize addMutex();

		/**
		 * @brief Removes mutex from pool.
		 */
		void removeMutex(usize);
	};
}
