#include "synchronization_primitives.hpp"

namespace vm {
	Ref<std::mutex> SynchronizationPrimitives::getMutex(usize mutex_id) {
		return mutex_pool.maybeGet(mutex_id).expect("Mutex does not exist");
	}

	usize SynchronizationPrimitives::addMutex() { return mutex_pool.add(); }

	void SynchronizationPrimitives::removeMutex(usize mutex_id) { mutex_pool.remove(mutex_id); }

	Ref<ConditionVariable> SynchronizationPrimitives::getCV(usize cv_id) {
		return cv_pool.get(cv_id);
	}

	usize SynchronizationPrimitives::addCV() { return cv_pool.add(); }

	void SynchronizationPrimitives::removeCV(usize cv_id) { cv_pool.remove(cv_id); }
}
