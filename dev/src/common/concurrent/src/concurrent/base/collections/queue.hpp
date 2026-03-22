#pragma once

#include <deque>
#include <type_traits>
#include <base/collections/optional.hpp>
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

            LockedIterator() = default;

            LockedIterator(std::shared_ptr<WithLock<AtomicFlagSpinlock>> lock_guard, internal_iterator_type iter) noexcept:
                lock_guard(std::move(lock_guard)), internal_iterator(iter) {}

            friend class ConQueue;
        private:
            std::shared_ptr<WithLock<AtomicFlagSpinlock>> lock_guard;
            internal_iterator_type internal_iterator;
        };

        using Iterator = LockedIterator<false>;
        using ConstIterator = LockedIterator<true>;

        static_assert(std::forward_iterator<Iterator>, "Iterator must be a forward iterator");
		static_assert(
			std::forward_iterator<ConstIterator>, "ConstIterator must be a forward iterator"
		);

        Iterator begin() {
            auto lock_guard = std::make_shared<WithLock<AtomicFlagSpinlock>>(&lock);
            return Iterator(lock_guard, queue.begin());
        }

        ConstIterator begin() const {
            auto lock_guard = std::make_shared<WithLock<AtomicFlagSpinlock>>(&lock);
            return ConstIterator(lock_guard, queue.cbegin());
        }

        Iterator end() {
            return Iterator(nullptr, queue.end());
        }

        ConstIterator end() const {
            return ConstIterator(nullptr, queue.cend());
        }

        Iterator erase(Iterator it) {
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