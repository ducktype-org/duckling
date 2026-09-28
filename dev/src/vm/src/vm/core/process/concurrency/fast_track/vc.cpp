/**
 * @file vc.cpp
 */

#include "vc.hpp"

#include "epoch.hpp"

#include <base/except/exceptions.hpp>

#include <vm/core/safe/exceptions.hpp>

#include <algorithm>

namespace vm {
	void VectorClock::ensureCapacity(api::ThreadID thread_id) {
		// A check rather than an assert: outside dev builds an assert vanishes, and a bad ID would
		// resize the clock to nothing and then index far past its end. The bad ID is `u64(-1)`,
		// so one comparison covers it and every ID an epoch cannot name.
		const u64 id = thread_id.asInt();
		if (id > Epoch::MAX_TID) [[unlikely]]
			throw exceptions::VMFastTrackLimitException(
				base::strConcat("vector clock indexed with a thread ID out of range: ", id)
			);
		if (id >= clocks.size()) clocks.resize(id + 1, 0);
	}

	Epoch::Clock VectorClock::operator[](api::ThreadID thread_id) const {
		u64 id = thread_id.asInt();
		if (id < clocks.size()) return clocks[id];
		return 0;
	}

	Epoch::Clock& VectorClock::operator[](api::ThreadID thread_id) {
		ensureCapacity(thread_id);
		return clocks[thread_id.asInt()];
	}

	VectorClock& VectorClock::operator|=(const VectorClock& other) {
		if (other.clocks.size() > clocks.size()) clocks.resize(other.clocks.size(), 0);
		for (usize i = 0; i < other.clocks.size(); ++i)
			clocks[i] = std::max(clocks[i], other.clocks[i]);
		return *this;
	}

	VectorClock& VectorClock::operator|=(const Epoch& other) {
		if (!other.tid().isGood()) return *this;
		Epoch::Clock& component = (*this)[other.tid()];
		component               = std::max(component, other.clock());
		return *this;
	}

	bool VectorClock::operator<=(const VectorClock& other) const {
		usize max_size = std::max(clocks.size(), other.clocks.size());
		for (usize i = 0; i < max_size; ++i) {
			Epoch::Clock this_val  = (i < clocks.size()) ? clocks[i] : 0;
			Epoch::Clock other_val = (i < other.clocks.size()) ? other.clocks[i] : 0;
			if (this_val > other_val) return false;
		}
		return true;
	}

	void VectorClock::increment(api::ThreadID thread_id) {
		Epoch::Clock& component = (*this)[thread_id];
		if (component == Epoch::MAX_CLOCK) [[unlikely]]
			throw exceptions::VMFastTrackLimitException("vector clock overflow");
		++component;
	}

	bool VectorClock::operator==(const VectorClock& other) const {
		usize max_size = std::max(clocks.size(), other.clocks.size());
		for (usize i = 0; i < max_size; ++i) {
			Epoch::Clock this_val  = (i < clocks.size()) ? clocks[i] : 0;
			Epoch::Clock other_val = (i < other.clocks.size()) ? other.clocks[i] : 0;
			if (this_val != other_val) return false;
		}
		return true;
	}
}
