#include "condition_variable.hpp"

#include <vm/core/safe/exceptions.hpp>

namespace vm {
	void ConditionVariable::wait(std::mutex& mutex) {
		std::mutex* expected  = nullptr;
		std::mutex* mutex_ptr = &mutex;
		if (!bound_mutex.compare_exchange_strong(expected, mutex_ptr) && expected != mutex_ptr)
			throw exceptions::VMRuntimeException(
				"ConditionVariable is already bound to a different mutex"
			);

		std::unique_lock<std::mutex> lock(mutex, std::adopt_lock);
		cv.wait(lock);
		lock.release();
	}

	void ConditionVariable::notifyOne() { cv.notify_one(); }

	void ConditionVariable::notifyAll() { cv.notify_all(); }
}
