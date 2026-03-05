#include "synchronization_primitives.hpp"

namespace vm {
	SharedBox<std::mutex> SynchronizationPrimitives::getMutex(i64 mutex_id) {
		return mutex_pool.get(mutex_id);
	}

	i64 SynchronizationPrimitives::addMutex() { return mutex_pool.add(); }

	void SynchronizationPrimitives::removeMutex(i64 mutex_id) { mutex_pool.remove(mutex_id); }
}
