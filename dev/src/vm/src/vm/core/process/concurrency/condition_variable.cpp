#include "condition_variable.hpp"

#include <stdexcept>

namespace vm {
	void ConditionVariable::wait(std::mutex& mutex) {
		std::mutex* expected = nullptr;
		if (!bound_mutex.compare_exchange_strong(expected, &mutex) && expected != &mutex) {
			throw std::logic_error("ConditionVariable is already bound to a different mutex");
		}

		std::unique_lock lock(mutex, std::adopt_lock);
		cv.wait(lock);
		lock.release();
	}

	void ConditionVariable::notifyOne() { cv.notify_one(); }

	void ConditionVariable::notifyAll() { cv.notify_all(); }
}
