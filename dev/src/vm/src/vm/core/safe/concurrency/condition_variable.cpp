#include "condition_variable.hpp"

#include <chrono>

#include <vm/core/safe/exceptions.hpp>

namespace vm {
	bool ConditionVariable::wait(std::mutex& mutex, const std::function<bool()>& should_interrupt) {
		std::mutex* expected  = nullptr;
		std::mutex* mutex_ptr = &mutex;
		if (!bound_mutex.compare_exchange_strong(expected, mutex_ptr) && expected != mutex_ptr)
			throw exceptions::VMRuntimeException(
				"ConditionVariable is already bound to a different mutex"
			);

		std::unique_lock<std::mutex> lock(mutex, std::adopt_lock);
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
