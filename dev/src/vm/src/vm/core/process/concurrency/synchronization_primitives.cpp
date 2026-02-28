#include "synchronization_primitives.hpp"

#include <vm/core/process/exceptions.hpp>

namespace vm {
	SharedBox<std::mutex> SynchronizationPrimitives::getMutex(i64 mutex_id) {
		if (mutex_map.find(mutex_id) == mutex_map.end()) throw exceptions::VMMutexDoesntExist();
		return mutex_map.at(mutex_id);
	}

	i64 SynchronizationPrimitives::addMutex() {
		mutex_map.put(next_mutex_id, base::makeSharedBox<std::mutex>());
		return next_mutex_id++;
	}

	void SynchronizationPrimitives::removeMutex(i64 mutex_id) {
		if (mutex_map.find(mutex_id) == mutex_map.end()) throw exceptions::VMMutexDoesntExist();
		mutex_map.erase(mutex_id);
	}
}
