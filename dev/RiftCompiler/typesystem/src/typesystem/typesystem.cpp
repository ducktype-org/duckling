#include "typesystem.hpp"
#include "internal/type_info_impl.hpp"

namespace ts {
	static bool was_init = false;

	void init() {
		if (was_init) return;
		was_init = true;
	}

	void reset() { internal::getTypes().clear(); }
}
