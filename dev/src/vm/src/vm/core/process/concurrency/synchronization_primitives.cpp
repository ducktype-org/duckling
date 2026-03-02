#include "synchronization_primitives.hpp"
#include <condition_variable>
#include "base/pointers/shared_box.hpp"

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

	SharedBox<std::condition_variable_any> SynchronizationPrimitives::getCV(i64 cv_id) {
		if (cv_map.find(cv_id) == cv_map.end()) throw exceptions::VMConditionVariableDoesntExist();
		return cv_map.at(cv_id);
	}

	i64 SynchronizationPrimitives::addCV() {
		cv_map.put(next_cv_id, base::makeSharedBox<std::condition_variable_any>());
		return next_cv_id++;
	}

	void SynchronizationPrimitives::removeCV(i64 cv_id) {
		if (cv_map.find(cv_id) == cv_map.end()) throw exceptions::VMConditionVariableDoesntExist();
		cv_map.erase(cv_id);
	}
}
