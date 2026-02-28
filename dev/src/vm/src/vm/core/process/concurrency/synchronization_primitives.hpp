#include <mutex>
#include <base/collections/maps.hpp>
#include <base/pointers/shared_box.hpp>
#include <base/types/ints.hpp>


namespace vm {
    class SynchronizationPrimitives final {

    private:
        /**
		 * @brief ID of a new mutex that's gonna be added to mutex pool.
		 */
		i64 next_mutex_id = 0;

		/**
		 * @brief Pool for mutexes used in the process. In the future they should be reusable.
		 */
		base::HashMap<i64, SharedBox<std::mutex>>
			mutex_map;  // @TODO: #2109 Find better structure then map for storing mutexes.

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