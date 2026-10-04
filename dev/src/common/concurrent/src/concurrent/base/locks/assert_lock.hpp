#pragma once

#include <base/config/build_type.hpp>
#include <base/except/exceptions.hpp>
#include <base/misc/noexcept.hpp>

#include <atomic>
#include <string_view>
#include <version>

namespace concurrent {

	/**
	 * This is a simple lock implementation based on atomic_flag.
	 * This locks assert that only one thread can acquire them at a time.
	 * Use this when you think that two threads should not be accessing the same resource at the
	 * same time and you want to assert that this is the case to avoid race conditions.
	 */
	class AssertLock final {
		IF_BUILD_TYPE_DEV(std::atomic_flag atomic_flag{ false };)

		IF_BUILD_TYPE_DEV(std::string_view panic_message
		                  = "AssertLock: lock already acquired by another thread.";)

	public:
		AssertLock() = default;

		/**
		 * @param panic_message the message to panic with if the lock is already acquired by another
		 * thread. Use this to make it easier to identify the source of the panic
		 * @note In release builds this lock works like a AtomicFlagSpinlock
		 */
		IF_BUILD_TYPE_DEV(AssertLock(std::string_view panic_message) : panic_message(panic_message
		){})

		IF_BUILD_TYPE_RELEASE(AssertLock([[maybe_unused]] std::string_view panic_message){})

		/**
		 * Acquires the lock, spinning and/or sleeping if necessary.
		 */
		void lock() RELEASE_NOEXCEPT {
			IF_BUILD_TYPE_DEV({
				bool prev_state = atomic_flag.test_and_set(std::memory_order_acquire);
				CORE_ASSERT(!prev_state, panic_message);
			})
		}

		/**
		 * Releases the lock.
		 */
		void unlock() noexcept {
			IF_BUILD_TYPE_DEV({ atomic_flag.clear(std::memory_order_release); })
		}
	};
}
