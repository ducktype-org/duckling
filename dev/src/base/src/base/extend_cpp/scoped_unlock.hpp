/**
 * @file scoped_unlock.hpp
 *
 * @brief `ScopedUnlock` is the inverse of `std::lock_guard`. It releases an already-held lock for
 * the duration of a scope and re-acquires it on scope exit.
 *
 * Meant to replace the manual `lock.unlock(); ...; lock.lock();` pattern.
 */

#pragma once

namespace base {
	/**
	 * @brief RAII guard that unlocks `lockable` on construction and re-locks it on destruction.
	 * `lockable` must be locked when the guard is constructed, and must outlive the guard.
	 *
	 * @tparam Lockable Any type with `lock()` / `unlock()`.
	 *
	 */
	template<typename Lockable>
	class ScopedUnlock final {
	public:
		explicit ScopedUnlock(Lockable& lockable): lockable(lockable) { lockable.unlock(); }

		~ScopedUnlock() { lockable.lock(); }

		ScopedUnlock(const ScopedUnlock&)            = delete;
		ScopedUnlock& operator=(const ScopedUnlock&) = delete;
		ScopedUnlock(ScopedUnlock&&)                 = delete;
		ScopedUnlock& operator=(ScopedUnlock&&)      = delete;

	private:
		Lockable& lockable;
	};
}
