#pragma once

#include <timer/timer.hpp>

namespace compiler::driver {
	/**
	 * Time spent on backend compilation.
	 * Used for time statistics collection.
	 * Updated via RAII helper.
	 * @note it doesn't measure it based on the backend modules,
	 * but based on driver operations.
	 * Any non-driver operations are not measured.
	 * @note it may be, in the future, moved to some other, more generic place.
	 * \parallel Non-atomic Duration updates are data races if multiple threads compile concurrently.
	 * Code getter: \ref dev/src/compiler/driver/driver/src/driver/statistics/statistics.cpp
	 * Code use-sites:
	 * - \ref dev/src/compiler/driver/driver/src/driver/operations/generic_operations.cpp
	 * - \ref dev/src/compiler/driver/driver/src_private/driver_private/backend_operations/compile_llvm.cpp
	 * - \ref dev/src/compiler/driver/driver/src_private/driver_private/backend_operations/compile_dvm.cpp
	 */
	extern timer::Duration backend_compilation_time;
}
