// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>

namespace vm {

	class ConditionVariable final {
	private:
		std::condition_variable_any    cv;
		std::atomic<std::timed_mutex*> bound_mutex{ nullptr };

	public:
		/**
		 * @brief Waits until the condition variable is notified or interruption is requested.
		 *
		 * The mutex must already be locked by the caller. This function temporarily releases it
		 * while waiting and re-acquires it before returning.
		 *
		 * @param mutex The mutex that guards the condition being waited on.
		 * @param should_interrupt Callback used to abort the wait, e.g. when thread termination is
		 * requested.
		 * @return True if the wait was interrupted, false if it was notified.
		 */
		bool wait(std::timed_mutex& mutex, const std::function<bool()>& should_interrupt);
		void notifyOne();
		void notifyAll();
	};
}
