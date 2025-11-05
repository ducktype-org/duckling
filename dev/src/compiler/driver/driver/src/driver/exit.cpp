#include "exit.hpp"

#include <driver_private/save_artifacts.hpp>
#include <global_state/artifacts_location.hpp>

namespace compiler::driver {

	void exit() {
		if (global_state::hasRootCollection()) saveArtifacts();
	}

}  // namespace compiler::driver
