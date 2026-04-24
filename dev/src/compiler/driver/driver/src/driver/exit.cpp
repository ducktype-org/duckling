#include "exit.hpp"

#include <driver/module_flags/module_flags.hpp>
#include <driver_private/save_artifacts.hpp>
#include <global_state/artifacts_location.hpp>
#include <time_stats/time_stats.hpp>

#include <query_framework/module_flags/module_flags.hpp>

namespace compiler::driver {

	void exit() {
		time_stats::TrackCategoryTime driver_exit_time(time_stats::TimeCategories::DriverExit);

		// For now saveArtifacts() saves the query graph needed for incremental compilation.
		// In the future, other driver-managed data may be saved here as well.
		if (!query::enable_incremental_compilation) return;

		if (global_state::hasRootCollection()) saveArtifacts();
	}

}  // namespace compiler::driver
