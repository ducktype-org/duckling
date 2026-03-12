#include "synchronization_primitives.hpp"

#include <vm/core/process/exceptions.hpp>

namespace vm {
	Ref<std::mutex> SynchronizationPrimitives::getMutex(usize mutex_id) {
		return mutex_pool.maybeGet(mutex_id).expect<exceptions::VMResourceDoesNotExist>("mutex");
	}

	usize SynchronizationPrimitives::addMutex() { return mutex_pool.add(); }

	void SynchronizationPrimitives::removeMutex(usize mutex_id) { mutex_pool.remove(mutex_id); }
}
