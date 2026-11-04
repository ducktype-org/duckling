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

#include <query_framework/context/context.hpp>
#include <query_framework/module_flags/module_flags.hpp>
#include <query_framework/q_stats/q_stats.hpp>

#include <iostream>

namespace compiler::driver {

	void exit() {
		time_stats::TrackCategoryTime driver_exit_time(time_stats::TimeCategories::DriverExit);

		if (global_state::hasRootCollection()) saveArtifacts();
		// @TODO: #3158 Validate the std artifacts collection here as well. A more generic
		// system will likely be needed to manage this with multipackage builds.
		if_opt_some(global_state::getStdArtifactsCollection(), std_art_collection) {
			std_art_collection->flush();
		}

		driver_exit_time.end();

		if (driver::print_statistics) {
			if (not query::USE_STATS) {
				std::cerr << "Warning: Query statistics are disabled at compile time. "
							 "No query statistics will be printed.\n";
			}
			query::printStats();
			time_stats::prettyPrintTimeStatistics();
		}

		if (driver::print_query_graph)
			query::Context::getState().getGraph().debugPrintForDrawing(std::cerr);
	}

}  // namespace compiler::driver
