/**
 * @file vc.cpp
 */

#include "vc.hpp"

#include "epoch.hpp"

#include <algorithm>

namespace vm {
	void VectorClock::ensureCapacity(api::ThreadID thread_id) {
		u64 id = thread_id.asInt();
		if (id >= clocks.size()) clocks.resize(id + 1, 0);
	}

	i32 VectorClock::operator[](api::ThreadID thread_id) const {
		u64 id = thread_id.asInt();
		if (id < clocks.size()) return clocks[id];
		return 0;
	}

	i32& VectorClock::operator[](api::ThreadID thread_id) {
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
		(*this)[other.tid()] = std::max((*this)[other.tid()], other.clock());
		return *this;
	}

	bool VectorClock::operator<=(const VectorClock& other) const {
		usize max_size = std::max(clocks.size(), other.clocks.size());
		for (usize i = 0; i < max_size; ++i) {
			i32 this_val  = (i < clocks.size()) ? clocks[i] : 0;
			i32 other_val = (i < other.clocks.size()) ? other.clocks[i] : 0;
			if (this_val > other_val) return false;
		}
		return true;
	}

	void VectorClock::increment(api::ThreadID thread_id) { ++(*this)[thread_id]; }

	bool VectorClock::operator==(const VectorClock& other) const {
		usize max_size = std::max(clocks.size(), other.clocks.size());
		for (usize i = 0; i < max_size; ++i) {
			i32 this_val  = (i < clocks.size()) ? clocks[i] : 0;
			i32 other_val = (i < other.clocks.size()) ? other.clocks[i] : 0;
			if (this_val != other_val) return false;
		}
		return true;
	}

}
