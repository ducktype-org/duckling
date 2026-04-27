/**
 * @file epoch.cpp
 */

#include "epoch.hpp"
#include "vc.hpp"

namespace vm {
	void Epoch::increment() { ++clock_; }

	bool Epoch::operator<=(const VectorClock& vc) const {
		return clock_ <= vc[tid_];
	}
}
