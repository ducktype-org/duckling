#include "synchronization_primitives.hpp"

#include <vm/core/safe/exceptions.hpp>

namespace vm {
	Ref<std::timed_mutex> SynchronizationPrimitives::getMutex(usize mutex_id) {
		return mutex_pool.maybeGet(mutex_id).expect<exceptions::VMResourceDoesNotExist>("mutex");
	}

	usize SynchronizationPrimitives::addMutex() { return mutex_pool.add(); }

	void SynchronizationPrimitives::removeMutex(usize mutex_id) { mutex_pool.remove(mutex_id); }

	Ref<ConditionVariable> SynchronizationPrimitives::getCV(usize cv_id) {
		return cv_pool.maybeGet(cv_id).expect<exceptions::VMResourceDoesNotExist>(
			"conditional variable"
		);
	}

	usize SynchronizationPrimitives::addCV() { return cv_pool.add(); }

	void SynchronizationPrimitives::removeCV(usize cv_id) {
		auto cv = cv_pool.get(cv_id);
		cv->reset();
		cv_pool.remove(cv_id);
	}
}
