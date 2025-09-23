#pragma once

#include "utils.hpp"

#include <base/ints.hpp>
#include <base/ref.hpp>

#include <atomic>

namespace concurrent {
	/**
	 * @note in the future, optimizing the memory order could result in much better performance,
	 * especially on ARM.
	 */
	struct AtomicU64 final {
	private:
		std::atomic<u64> value;
		static_assert(std::atomic<u64>::is_always_lock_free, "u64 is not lock-free");

	public:
		AtomicU64(): value(0) {}

		AtomicU64(u64 value): value(value) {}

		[[nodiscard]]
		u64 load() const noexcept {
			return value.load();
		}

		void store(u64 desired) noexcept { value.store(desired); }

		auto inc() noexcept { return value++; }

		auto dec() noexcept { return value--; }

		auto add(u64 v) noexcept { return value.fetch_add(v); }

		auto sub(u64 v) noexcept { return value.fetch_sub(v); }

		CmpRes cmpAndSwap(u64 expected, u64 desired) {
			bool res = value.compare_exchange_strong(expected, desired);
			return res ? CmpRes::Changed : CmpRes::NotChanged;
		}

		CmpRes cmpAndSwap(u64 expected, u64 desired, Ref<u64> actual) {
			bool res = value.compare_exchange_strong(expected, desired);
			*actual  = expected;
			return res ? CmpRes::Changed : CmpRes::NotChanged;
		}

		/**
		 * Decrement the value if it is not zero.
		 * Does not avoid the ABA problem, if unlucky, can fail randomly.
		 */
		CmpRes decIfNonZero() {
			// @OPT this could be a big deal
			u64 expected = value.load();
			if (expected == 0) return CmpRes::NotChanged;
			return cmpAndSwap(expected, expected - 1);
		}
	};


}
