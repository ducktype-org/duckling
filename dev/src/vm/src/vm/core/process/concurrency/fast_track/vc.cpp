/**
 * @file vc.cpp
 */

#include "vc.hpp"

#include "epoch.hpp"

#include <base/except/exceptions.hpp>

#include <algorithm>

namespace vm {
	void VectorClock::ensureCapacity(api::ThreadID thread_id) {
		CORE_ASSERT(thread_id.isGood(), "Vector clock indexed with a bad thread ID");
		u64 id = thread_id.asInt();
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

	void VectorClock::increment(api::ThreadID thread_id) { ++(*this)[thread_id]; }

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
