// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "synchronization_primitives.hpp"

#include <os_utils/timed_mutex_recovery.hpp>

#include <vm/core/safe/exceptions.hpp>

namespace vm {
	SynchronizationPrimitives::~SynchronizationPrimitives() {
		// A DVM thread that panicked or was killed while holding a mutex leaves it locked; clear
		// it so the pooled timed_mutex is not destroyed while locked (see clearAbandonedLock).
		for (auto& mutex: mutex_pool) os_utils::clearAbandonedLock(mutex);
	}

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

	void SynchronizationPrimitives::removeCV(usize cv_id) { cv_pool.remove(cv_id); }
}
