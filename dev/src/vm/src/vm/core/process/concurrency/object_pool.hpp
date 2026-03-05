#pragma once

#include <base/collections/maps.hpp>
#include <base/pointers/shared_box.hpp>
#include <base/types/ints.hpp>

#include <vm/core/process/exceptions.hpp>

#include <queue>
#include <vector>

namespace vm {

	/**
	 * @brief Universal pool for objects of any type.
	 *
	 * The class stores objects of type T in a pool and allows:
	 *  - adding new objects,
	 *  - retrieving objects by ID,
	 *  - removing objects (freeing slots).
	 *
	 * Slots are reused using the free_ids queue.
	 */
	template<typename T>
	class ObjectPool final {
	private:
		/**
		 * @brief Pool for objects used in the process.
		 */
		std::vector<SharedBox<T>> object_pool;

		/**
		 * @brief Indicates which object_pool slots are currently unused.
		 */
		std::vector<bool> is_free;

		/**
		 * @brief Pool of free object indices.
		 */
		std::queue<i64> free_ids;

		/**
		 * @brief Maximum number of objects that can be stored in the object pool.
		 */
		i64 max_objects;

		/**
		 * @brief Default maximum number of objects if not specified.
		 */
		static constexpr i64 DEFAULT_MAX_OBJECTS = 1e6;

	public:
		explicit ObjectPool(i64 max_objects = DEFAULT_MAX_OBJECTS): max_objects(max_objects) {}

		/**
		 * @brief Constructs an ObjectPool with a given maximum capacity.
		 *
		 * @param max_objects Maximum number of objects allowed in the pool.
		 *                    Defaults to DEFAULT_MAX_OBJECTS.
		 */
		SharedBox<T> get(i64 object_id) {
			if (object_id >= (i64) object_pool.size() || is_free.at((size_t) object_id))
				throw exceptions::VMObjectDoesntExist();

			return object_pool.at((size_t) object_id);
		}

		/**
		 * @brief Adds new object into pool.
		 */
		i64 add() {
			if (free_ids.size() == 0) {
				if ((i64) object_pool.size() >= max_objects) throw exceptions::VMTooManyObjects();

				free_ids.push((i64) object_pool.size());
				object_pool.push_back(base::makeSharedBox<T>());
				is_free.push_back(false);
			}

			i64 free_id = free_ids.front();
			free_ids.pop();
			return free_id;
		}

		/**
		 * @brief Removes object from pool.
		 */
		void remove(i64 object_id) {
			if (object_id >= (i64) object_pool.size() || is_free.at((size_t) object_id))
				throw exceptions::VMObjectDoesntExist();

			is_free.at((size_t) object_id) = true;
			free_ids.push(object_id);
		}
	};

}
