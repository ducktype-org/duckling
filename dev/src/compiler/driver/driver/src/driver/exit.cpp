#include "exit.hpp"

#include <driver_private/save_artifacts.hpp>
#include <global_state/artifacts_location.hpp>
#include <time_stats/time_stats.hpp>

namespace compiler::driver {

	void exit() {
		time_stats::TrackCategoryTime driver_exit_time(time_stats::TimeCategories::DriverExit);

		if (global_state::hasRootCollection()) saveArtifacts();
	}

}  // namespace compiler::driver
