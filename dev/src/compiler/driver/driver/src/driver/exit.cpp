// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
		// @TODO: #3158 Validate the std artifacts collection here as well. A more generic
		// system will likely be needed to manage this with multipackage builds.
		if_opt_some(global_state::getStdArtifactsCollection(), std_art_collection) {
			std_art_collection->flush();
		}
	}

}  // namespace compiler::driver
