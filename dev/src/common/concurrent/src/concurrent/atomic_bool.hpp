#pragma once

#include "utils.hpp"

#include <base/ints.hpp>

#include <atomic>

namespace concurrent {

	struct AtomicBool final {
	private:
		// test: see what using atomic flag gives
		std::atomic<bool> value;
		static_assert(std::atomic<bool>::is_always_lock_free, "bool is not lock-free");

	public:
		AtomicBool(): value(false) {}

		AtomicBool(bool value): value(value) {}

		[[nodiscard]]
		bool load() const noexcept {
			return value.load();
		}

		void store(bool desired) noexcept { value.store(desired); }

		[[nodiscard]]
		CmpRes cmpAndSwap(bool expected, bool desired) {
			bool res = value.compare_exchange_strong(expected, desired);
			return res ? CmpRes::Changed : CmpRes::NotChanged;
		}
	};

}
