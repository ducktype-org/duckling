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
		bool wait(std::timed_mutex& mutex, const std::function<bool()>& should_interrupt);
		void notifyOne();
		void notifyAll();
	};
}
