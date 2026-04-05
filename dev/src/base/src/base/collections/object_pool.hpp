#pragma once

#include <base/collections/maps.hpp>
#include <base/except/exceptions.hpp>
#include <base/types/ints.hpp>

#include <functional>
#include <queue>

namespace base {

	/**
	 * @brief Universal pool for objects of any type.
	 *
	 * The class stores objects of type T in a pool and allows:
	 *  - adding new objects,
	 *  - retrieving objects by ID with a stable reference,
	 *  - removing objects (freeing slots).
	 *
	 * Slots are reused using the free_ids queue.
	 */
	template<class T, class ObjID = u64, bool SHOULD_RECYCLE = true>
	class StableObjectPool final {
	private:
		/**
		 * @brief Pool for objects.
		 */
		std::deque<T> object_pool;

		/**
		 * @brief Indicates which object_pool slots are currently unused.
		 */
		std::deque<bool> is_free;

		/**
		 * @brief Pool of free object indices.
		 */
		std::queue<ObjID> free_ids;

		/**
		 * @brief Maximum number of objects that can be stored in the object pool.
		 */
		usize max_objects;

		/**
		 * @brief Default maximum number of objects if not specified.
		 */
		static constexpr usize DEFAULT_MAX_OBJECTS = 1'000'000;

		/**
		 * @brief Default object constructor, which creates a default object of type T.
		 */
		std::function<T(ObjID)> object_constructor;

	public:
		/**
		 * @brief Constructs an ObjectPool with a given maximum capacity.
		 *
		 * @param object_constructor Function to construct new objects, given their ObjID.
		                               Defaults to a function that creates default objects of type T.
		 * @param max_objects Maximum number of objects allowed in the pool.
		 *                    Defaults to DEFAULT_MAX_OBJECTS.
		 * @note Returned references are stable.
		 */
		explicit StableObjectPool(
			std::function<T(ObjID)> object_constructor = [](ObjID) { return T(); },
			usize                   max_objects        = DEFAULT_MAX_OBJECTS
		):
			  object_constructor(object_constructor),
			  max_objects(max_objects) {}

		/**
		 * @brief Sets the constructor function for new objects.
		 *
		 * @param new_constructor Function to construct new objects, given their ObjID.
		 */
		void setConstructor(std::function<T(ObjID)> new_constructor) {
			object_constructor = new_constructor;
		}

		/**
		 * @brief Attempts to retrieve an object by its ID.
		 *
		 * @param object_id ID of the object in the pool.
		 * @return Optional containing a reference to the object if it exists,
		 *         otherwise an empty Optional.
		 */
		base::Optional<Ref<T>> maybeGet(ObjID object_id) {
			if (object_id < object_pool.size() && !is_free.at(object_id))
				return &object_pool.at(object_id);
			return {};
		}

		base::Optional<CRef<T>> maybeGet(ObjID object_id) const {
			if (object_id < object_pool.size() && !is_free.at(object_id))
				return &object_pool.at(object_id);
			return {};
		}

		/**
		 * @brief Retrieves an object by its ID.
		 *
		 * @param object_id ID of the object in the pool.
		 * @return Reference to the object stored in the pool.
		 *
		 * @throws Exception if the object does not exist.
		 */
		Ref<T> get(ObjID object_id) { return maybeGet(object_id).expect("Object does not exist"); }

		CRef<T> get(ObjID object_id) const {
			return maybeGet(object_id).expect("Object does not exist");
		}

		/**
		 * @brief Adds new object into pool.
		 * @return ObjID ID of the newly created object in the pool.
		 */
		ObjID add() {
			CORE_ASSERT(object_pool.size() < max_objects, "Too many objects in the pool");
			if constexpr (SHOULD_RECYCLE) {
				if (!free_ids.empty()) {
					ObjID free_id = free_ids.front();
					free_ids.pop();
					object_pool[free_id]                 = object_constructor(free_id);
					is_free[static_cast<usize>(free_id)] = false;
					return free_id;
				}
			}
			ObjID new_id(object_pool.size());
			object_pool.push_back(std::move(object_constructor(new_id)));
			is_free.push_back(false);
			return new_id;
		}

		/**
		 * @brief Removes object from pool.
		 */
		void remove(ObjID object_id) {
			CORE_ASSERT(
				object_id < object_pool.size() && !is_free.at((size_t) object_id),
				"Object is already removed"
			);

			is_free.at((size_t) object_id) = true;
			free_ids.push(object_id);
		}
	};

}
