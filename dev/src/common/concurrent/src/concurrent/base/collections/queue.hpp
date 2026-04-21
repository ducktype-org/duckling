#pragma once

#include <concurrent/base/locks/atomic_flag_spinlock.hpp>
#include <concurrent/base/locks/with_lock.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/pointers/ref.hpp>

#include <deque>
#include <type_traits>

namespace concurrent {
	template<typename DATA_T>
	class ConQueue final {
	public:
		/**
		 * @brief Tries to remove and return the front element.
		 * @return Empty optional when the queue is empty.
		 */
		[[nodiscard]]
		base::Optional<DATA_T> tryPop() {
			WithLock scoped_lock(&lock);
			if (queue.empty()) return {};
			DATA_T data = std::move(queue.front());
			queue.pop_front();
			return data;
		}

		/**
		 * @brief Tries to pop the front element if it matches @param pred.
		 * @return Empty optional when the queue is empty or predicate fails.
		 * @note Contract: once @p pred evaluates to true for an element in this call,
		 * that same element is extracted and returned before releasing the queue lock.
		 * It cannot be removed concurrently by another thread between match and extraction.
		 * Parts of the compiler implementation rely on this guarantee.
		 */
		template<typename Predicate>
		[[nodiscard]]
		base::Optional<DATA_T> tryPopIf(Predicate&& pred) {
			WithLock scoped_lock(&lock);
			if (queue.empty()) return {};
			if (std::forward<Predicate>(pred)(base::CRef<DATA_T>(&queue.front()))) {
				DATA_T data = std::move(queue.front());
				queue.pop_front();
				return data;
			}
			return {};
		}

		/**
		 * @brief Finds and removes the first element matching @p pred.
		 * @return Extracted element when found, otherwise empty optional.
		 * @note Contract: once @p pred evaluates to true for an element in this call,
		 * that same element is extracted and returned before releasing the queue lock.
		 * It cannot be removed concurrently by another thread between match and extraction.
		 * Parts of the compiler implementation rely on this guarantee.
		 */
		template<typename Predicate>
		[[nodiscard]]
		base::Optional<DATA_T> extractIf(Predicate&& pred) {
			WithLock scoped_lock(&lock);
			if (queue.empty()) return {};
			auto&& pred_ref = std::forward<Predicate>(pred);

			auto it = std::find_if(queue.begin(), queue.end(), [&](const DATA_T& elem) {
				return pred_ref(base::CRef<DATA_T>(&elem));
			});

			if (it == queue.end()) return {};

			DATA_T data = std::move(*it);
			queue.erase(it);
			return data;
		}

		/**
		 * @brief Pushes an element to the back of the queue.
		 */
		void push(DATA_T data) {
			WithLock scoped_lock(&lock);
			queue.push_back(std::move(data));
		}

		/**
		 * @brief Checks whether the queue is empty.
		 */
		[[nodiscard]]
		bool empty() const {
			WithLock scoped_lock(&lock);
			return queue.empty();
		}

		/**
		 * @brief Returns the current number of elements.
		 */
		[[nodiscard]]
		usize size() const {
			WithLock scoped_lock(&lock);
			return queue.size();
		}

		template<bool IS_CONST>
		struct LockedIterator final {
			using internal_iterator_type = std::conditional_t<
				IS_CONST,
				typename std::deque<DATA_T>::const_iterator,
				typename std::deque<DATA_T>::iterator>;

		public:
			// Aliases required by STL
			using iterator_category = std::forward_iterator_tag;
			using difference_type   = std::ptrdiff_t;
			using value_type        = DATA_T;
			using pointer           = std::conditional_t<IS_CONST, const value_type*, value_type*>;
			using reference         = std::conditional_t<IS_CONST, const value_type&, value_type&>;

			reference operator*() const { return *internal_iterator; }

			pointer operator->() const { return &(*internal_iterator); }

			LockedIterator& operator++() {
				++internal_iterator;
				return *this;
			}

			LockedIterator operator++(int) {
				LockedIterator tmp = *this;
				++(*this);
				return tmp;
			}

			friend bool operator==(const LockedIterator& a, const LockedIterator& b) {
				return a.internal_iterator == b.internal_iterator;
			}

			friend bool operator!=(const LockedIterator& a, const LockedIterator& b) {
				return !(a == b);
			}

			LockedIterator() noexcept = default;

			LockedIterator(
				std::shared_ptr<WithLock<AtomicFlagSpinlock>> lock_guard, internal_iterator_type iter
			) noexcept:
				  lock_guard(std::move(lock_guard)),
				  internal_iterator(iter) {}

			friend class ConQueue;

		private:
			std::shared_ptr<WithLock<AtomicFlagSpinlock>> lock_guard;
			internal_iterator_type                        internal_iterator;
		};

		using Iterator      = LockedIterator<false>;
		using ConstIterator = LockedIterator<true>;

		static_assert(std::forward_iterator<Iterator>, "Iterator must be a forward iterator");
		static_assert(
			std::forward_iterator<ConstIterator>, "ConstIterator must be a forward iterator"
		);

		/**
		 * @brief Returns an iterator range start holding the queue lock.
		 * @note The lock is released when the iterator is destroyed.
		 */
		Iterator begin() {
			auto lock_guard = std::make_shared<WithLock<AtomicFlagSpinlock>>(&lock);
			return Iterator(lock_guard, queue.begin());
		}

		/**
		 * @brief Returns a const iterator range start holding the queue lock.
		 */
		ConstIterator begin() const {
			auto lock_guard = std::make_shared<WithLock<AtomicFlagSpinlock>>(&lock);
			return ConstIterator(lock_guard, queue.cbegin());
		}

		/**
		 * @brief Returns an iterator sentinel that does not own the lock.
		 */
		Iterator end() { return Iterator(nullptr, queue.end()); }

		/**
		 * @brief Returns a const iterator sentinel that does not own the lock.
		 */
		ConstIterator end() const { return ConstIterator(nullptr, queue.cend()); }

	private:
		std::deque<DATA_T>         queue;
		mutable AtomicFlagSpinlock lock;
	};
}
