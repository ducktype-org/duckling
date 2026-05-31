#include "condition_variable.hpp"

#include <vm/core/safe/exceptions.hpp>

#include <chrono>

namespace vm {
	bool ConditionVariable::wait(
		std::timed_mutex& mutex, const std::function<bool()>& should_interrupt
	) {
		std::timed_mutex* expected  = nullptr;
		std::timed_mutex* mutex_ptr = &mutex;
		if (!bound_mutex.compare_exchange_strong(expected, mutex_ptr) && expected != mutex_ptr)
			throw exceptions::VMRuntimeException(
				"ConditionVariable is already bound to a different mutex"
			);

		std::unique_lock<std::timed_mutex> lock(mutex, std::adopt_lock);
		while (true) {
			if (should_interrupt()) return true;
			if (cv.wait_for(lock, std::chrono::milliseconds{ 1 }) == std::cv_status::no_timeout) {
				lock.release();
				return false;
			}
		}
	}

	void ConditionVariable::notifyOne() { cv.notify_one(); }

	void ConditionVariable::notifyAll() { cv.notify_all(); }
}
