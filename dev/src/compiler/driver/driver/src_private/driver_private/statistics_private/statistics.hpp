#pragma once

#include <timer/timer.hpp>

namespace compiler::driver {
	/**
	 * Time spent on backend compilation.
	 * @note it doesn't measure it based on the backend modules,
	 * but based on driver operations.
	 * Any non-driver operations are not measured.
	 */
	extern timer::Duration backend_compilation_time;
}
