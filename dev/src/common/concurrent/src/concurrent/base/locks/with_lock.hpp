#pragma once

#include <base/pointers/ref.hpp>

namespace concurrent {
	/**
	 * RAII wrapper for locking a lock of type LockType.
	 * It is an analogy of std::lock_guard, that can be safely used with out locks.
	 */
	template<typename LockType>
	struct WithLock final {
	private:
		Ref<LockType> lock;

	public:
		WithLock(Ref<LockType> lock): lock(lock) { this->lock->lock(); }

		~WithLock() { this->lock->unlock(); }
	};

	template<typename LockType>
	WithLock(Ref<LockType> lock) -> WithLock<LockType>;

	template<typename LockType>
	WithLock(LockType* lock) -> WithLock<LockType>;
}
