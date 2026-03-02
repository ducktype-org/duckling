#include "gil.hpp"

namespace vm {
	void GIL::acquire() { gil.lock(); }

	void GIL::release() {
		operations = 0;
		gil.unlock();
	}

	bool GIL::shouldRelease() {
		operations++;
		if (operations >= MAX_GIL_OPERATIONS) return true;
		return false;
	}
}
