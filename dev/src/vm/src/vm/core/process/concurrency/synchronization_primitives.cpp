#include "synchronization_primitives.hpp"

#include <vm/core/process/exceptions.hpp>

namespace vm {
	SharedBox<std::mutex> SynchronizationPrimitives::getMutex(i64 mutex_id) {
		if (auto it = mutex_map.find(mutex_id); it != mutex_map.end()) return (*it).second;
		throw exceptions::VMMutexDoesntExist();
	}

	i64 SynchronizationPrimitives::addMutex() {
		mutex_map.put(next_mutex_id, base::makeSharedBox<std::mutex>());
		return next_mutex_id++;
	}

	void SynchronizationPrimitives::removeMutex(i64 mutex_id) {
		if (auto it = mutex_map.find(mutex_id); it != mutex_map.end())
			mutex_map.erase(it);
		else
			throw exceptions::VMMutexDoesntExist();
	}
}
