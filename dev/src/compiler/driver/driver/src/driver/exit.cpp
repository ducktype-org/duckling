#include "exit.hpp"

#include <driver/module_flags/module_flags.hpp>
#include <driver_private/save_artifacts.hpp>
#include <global_state/artifacts_location.hpp>
#include <global_state/packages.hpp>
#include <time_stats/time_stats.hpp>

#include <query_framework/module_flags/module_flags.hpp>

namespace compiler::driver {

	void exit() {
		time_stats::TrackCategoryTime driver_exit_time(time_stats::TimeCategories::DriverExit);

		if (global_state::hasRootCollection()) saveArtifacts();
		if_opt_some(global_state::getStdArtifactsCollection(), std_art_collection) {
			std_art_collection->flush();
		}
	}

}  // namespace compiler::driver
