#pragma once

#include <timer/timer.hpp>

namespace compiler::driver {
	/**
	 * Time spent on backend compilation.
	 * Used for time statistics collection.
	 * @note it doesn't measure it based on the backend modules,
	 * but based on driver operations.
	 * Any non-driver operations are not measured.
	 * @note it may be, in the future, moved to some other, more generic place.
	 */
	extern timer::Duration backend_compilation_time;
}
