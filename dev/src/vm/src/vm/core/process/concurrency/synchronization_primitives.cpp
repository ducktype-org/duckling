#include "synchronization_primitives.hpp"

namespace vm {
	Ref<std::mutex> SynchronizationPrimitives::getMutex(i64 mutex_id) {
		return mutex_pool.get(mutex_id);
	}

	i64 SynchronizationPrimitives::addMutex() { return mutex_pool.add(); }

	void SynchronizationPrimitives::removeMutex(i64 mutex_id) { mutex_pool.remove(mutex_id); }

	Ref<ConditionVariable> SynchronizationPrimitives::getCV(i64 cv_id) {
		return cv_pool.get(cv_id);
	}

	i64 SynchronizationPrimitives::addCV() { return cv_pool.add(); }

	void SynchronizationPrimitives::removeCV(i64 cv_id) {
		cv_pool.remove(cv_id);
	}

}
