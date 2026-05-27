/**
 * @file epoch.cpp
 */

#include "epoch.hpp"

#include "vc.hpp"

namespace vm {
	void Epoch::increment() { ++clock_value; }

	bool Epoch::operator<=(const VectorClock& vc) const { return clock_value <= vc[thread_id]; }
}
