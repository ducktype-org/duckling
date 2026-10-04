#pragma once

#include <global_state/artifacts_location.hpp>

namespace compiler::driver {
	/**
	 * Exit the driver: persist artifacts and any driver-managed data (e.g. query graph),
	 * then flush to disk.
	 */
	void exit();
}
