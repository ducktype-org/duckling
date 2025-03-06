#include "all.hpp"
#include "internal/abstract_type_impl.hpp"

namespace tsh {
	static bool was_init = false;

	void init() {
		if (was_init) return;
		was_init = true;
	}

	void reset() { internal::getTypes().clear(); }
}
