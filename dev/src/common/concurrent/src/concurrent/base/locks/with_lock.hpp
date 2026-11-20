// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
