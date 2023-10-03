#include "typesystem.hpp"

namespace ts {
	static bool was_init = false;

	void init() {
		if (was_init) return;
		was_init = true;
	}
}  // namespace ts
