/**
 * @file epoch.cpp
 */

#include "epoch.hpp"

#include "vc.hpp"

namespace vm {
	bool Epoch::operator<=(const VectorClock& vc) const { return clock_value <= vc[thread_id]; }
}
