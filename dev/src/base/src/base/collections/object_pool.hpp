#pragma once

#include <base/collections/maps.hpp>
#include <base/except/exceptions.hpp>
#include <base/types/ints.hpp>

#include <functional>
#include <queue>
#include <type_traits>
#include <utility>

namespace base {

	/**
	 * @brief Iterator over valid (non-free) objects in a StableObjectPool.
	 *
	 * This iterator skips all free objects and only yields valid objects.
	 * It wraps deque iterators and maintains validity by checking the is_free
	 * tracking deque during iteration.
	 *
	 * @tparam T Type of objects stored in the pool.
	 * @tparam ObjID Type used for object identifiers.
	 */
	template<class T, class ObjID>
	class ValidObjectIterator {
	private:
		typename std::deque<T>::iterator current;
		typename std::deque<T>::iterator end;
		typename std::deque<T>::iterator begin;
		const std::deque<bool>&          is_free;

		/**
		 * @brief Advances iterator to the next valid (non-free) object.
		 */
		void skipFreeObjects() {
			while (current != end) {
				usize index = std::distance(begin, current);
				if (index < is_free.size() && !is_free[index]) break;
				++current;
			}
		}

	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type        = T;
		using difference_type   = std::ptrdiff_t;
		using pointer           = T*;
		using reference         = T&;

		/**
		 * @brief Constructs a ValidObjectIterator.
		 *
		 * @param current Iterator pointing to current position in object pool.
		 * @param end Iterator pointing to end of object pool.
		 * @param begin Iterator pointing to beginning of object pool.
		 * @param is_free Pointer to deque tracking which objects are free.
		 */
		ValidObjectIterator(
			typename std::deque<T>::iterator current,
			typename std::deque<T>::iterator end,
			typename std::deque<T>::iterator begin,
			const std::deque<bool>&          is_free
		):
			  current(current),
			  end(end),
			  begin(begin),
			  is_free(is_free) {
			skipFreeObjects();
		}

		/**
		 * @brief Dereferences the iterator.
		 * @return Reference to the current valid object.
		 */
		reference operator*() { return *current; }

		/**
		 * @brief Arrow operator.
		 * @return Pointer to the current valid object.
		 */
		pointer operator->() { return &(*current); }

		/**
		 * @brief Pre-increment operator.
		 * @return Reference to this iterator after advancing to next valid object.
		 */
		ValidObjectIterator& operator++() {
			++current;
			skipFreeObjects();
			return *this;
		}

		/**
		 * @brief Post-increment operator.
		 * @return Iterator pointing to the object before increment.
		 */
		ValidObjectIterator operator++(int) {
			ValidObjectIterator tmp = *this;
			++(*this);
			return tmp;
		}

		/**
		 * @brief Equality comparison.
		 * @param other Other iterator to compare with.
		 * @return True if both iterators point to the same position.
		 */
		bool operator==(const ValidObjectIterator& other) const { return current == other.current; }

		/**
		 * @brief Inequality comparison.
		 * @param other Other iterator to compare with.
		 * @return True if iterators point to different positions.
		 */
		bool operator!=(const ValidObjectIterator& other) const { return current != other.current; }
	};

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
	template<
		class T,
		class ObjID                 = u64,
		bool SHOULD_RECYCLE         = std::is_move_assignable_v<T>,
		bool PASS_ID_TO_CONSTRUCTOR = false>
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

	public:
		/**
		 * @brief Constructs an ObjectPool with a given maximum capacity.
		 *
		 * @param max_objects Maximum number of objects allowed in the pool.
		 *                    Defaults to DEFAULT_MAX_OBJECTS.
		 * @note Returned references are stable.
		 */
		explicit StableObjectPool(usize max_objects = DEFAULT_MAX_OBJECTS):
			  max_objects(max_objects) {}

		/**
		 * @brief Attempts to retrieve an object by its ID.
		 *
		 * @param object_id ID of the object in the pool.
		 * @return Optional containing a reference to the object if it exists,
		 *         otherwise an empty Optional.
		 */
		base::Optional<Ref<T>> maybeGet(ObjID object_id) {
			auto object_id_usize = static_cast<usize>(object_id);
			if (object_id_usize < object_pool.size() && !is_free.at(object_id_usize))
				return &object_pool.at(usize(object_id_usize));
			return {};
		}

		base::Optional<CRef<T>> maybeGet(ObjID object_id) const {
			auto object_id_usize = static_cast<usize>(object_id);
			if (object_id_usize < object_pool.size() && !is_free.at(object_id_usize))
				return &object_pool.at(usize(object_id_usize));
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
		template<class... Args>
		ObjID add(Args&&... args) {
			CORE_ASSERT(object_pool.size() < max_objects, "Too many objects in the pool");
			if constexpr (SHOULD_RECYCLE) {
				if (!free_ids.empty()) {
					ObjID free_id = free_ids.front();
					free_ids.pop();
					if constexpr (PASS_ID_TO_CONSTRUCTOR)
						object_pool[static_cast<usize>(free_id)]
							= T(free_id, std::forward<Args>(args)...);
					else
						object_pool[static_cast<usize>(free_id)] = T(std::forward<Args>(args)...);
					is_free[static_cast<usize>(free_id)] = false;
					return free_id;
				}
			}
			ObjID new_id(object_pool.size());
			if constexpr (PASS_ID_TO_CONSTRUCTOR)
				object_pool.emplace_back(new_id, std::forward<Args>(args)...);
			else
				object_pool.emplace_back(std::forward<Args>(args)...);
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

		/**
		 * @brief Returns an iterator to the first valid object in the pool.
		 *
		 * @return ValidObjectIterator pointing to the first non-free object,
		 *         or end() if no valid objects exist.
		 */
		ValidObjectIterator<T, ObjID> begin() {
			return ValidObjectIterator<T, ObjID>(
				object_pool.begin(), object_pool.end(), object_pool.begin(), is_free
			);
		}

		/**
		 * @brief Returns an iterator to the end of the pool.
		 *
		 * @return ValidObjectIterator pointing past the last object in the pool.
		 */
		ValidObjectIterator<T, ObjID> end() {
			return ValidObjectIterator<T, ObjID>(
				object_pool.end(), object_pool.end(), object_pool.begin(), is_free
			);
		}
	};

}
