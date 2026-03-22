#pragma once

#include <deque>
#include <type_traits>
#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/pointers/ref.hpp>
#include <concurrent/base/locks/atomic_flag_spinlock.hpp>
#include <concurrent/base/locks/with_lock.hpp>

namespace concurrent {
    template <typename DATA_T>
    class ConQueue final {
    public:
        [[nodiscard]]
        base::Optional<DATA_T> tryPop(){
            WithLock scoped_lock(&lock);
            if(queue.empty()) return {};
            DATA_T data = std::move(queue.front());
            queue.pop_front();
            return data;
        }


        template <typename Predicate>
        [[nodiscard]]
        base::Optional<DATA_T> tryPopIf(Predicate pred) {
            WithLock scoped_lock(&lock);
            if(queue.empty()) return {};
            if(pred(base::CRef<DATA_T>(&queue.front()))) {
                DATA_T data = std::move(queue.front());
                queue.pop_front();
                return data;
            }
            return {};
        }

        template <typename Predicate>
        [[nodiscard]]
        base::Optional<DATA_T> extractIf(Predicate pred) {
            WithLock scoped_lock(&lock);
            if (queue.empty()) return {};

            auto it = std::find_if(queue.begin(), queue.end(), [&](const DATA_T& elem) {
                return pred(base::CRef<DATA_T>(&elem));
            });

            if (it == queue.end()) return {};

            DATA_T data = std::move(*it);
            queue.erase(it);
            return data;
        }

        void push(DATA_T data){
            WithLock scoped_lock(&lock);
            queue.push_back(std::move(data));
        }

        [[nodiscard]]
        bool empty() const {
            WithLock scoped_lock(&lock);
            return queue.empty();
        }

        [[nodiscard]]
        usize size() const {
            WithLock scoped_lock(&lock);
            return queue.size();
        }

        template <bool IS_CONST>
        struct LockedIterator final {
            using internal_iterator_type = std::conditional_t<IS_CONST,
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
                std::shared_ptr<WithLock<AtomicFlagSpinlock>> lock_guard,
                internal_iterator_type iter,
                const ConQueue* owner
            ) noexcept:
                lock_guard(std::move(lock_guard)), internal_iterator(iter), owner(owner) {}

            friend class ConQueue;
        private:
            std::shared_ptr<WithLock<AtomicFlagSpinlock>> lock_guard;
            internal_iterator_type internal_iterator;
            const ConQueue* owner = nullptr;
        };

        using Iterator = LockedIterator<false>;
        using ConstIterator = LockedIterator<true>;

        static_assert(std::forward_iterator<Iterator>, "Iterator must be a forward iterator");
		static_assert(
			std::forward_iterator<ConstIterator>, "ConstIterator must be a forward iterator"
		);

        Iterator begin() {
            auto lock_guard = std::make_shared<WithLock<AtomicFlagSpinlock>>(&lock);
            return Iterator(lock_guard, queue.begin(), this);
        }

        ConstIterator begin() const {
            auto lock_guard = std::make_shared<WithLock<AtomicFlagSpinlock>>(&lock);
            return ConstIterator(lock_guard, queue.cbegin(), this);
        }

        Iterator end() {
            return Iterator(nullptr, queue.end(), this);
        }

        ConstIterator end() const {
            return ConstIterator(nullptr, queue.cend(), this);
        }

        Iterator erase(Iterator it) {
            CORE_ASSERT(it.owner == this, "Erasing with iterator from different queue");
            CORE_ASSERT(it.lock_guard != nullptr, "Erasing requires a locked iterator");
            CORE_ASSERT(it.internal_iterator != queue.end(), "Erasing end iterator is not allowed");
            auto internal_it = it.internal_iterator;
            internal_it = queue.erase(internal_it);
            it.internal_iterator = internal_it;
            return it;
        }
    private:
        std::deque<DATA_T> queue;
        mutable AtomicFlagSpinlock lock;
    };
}