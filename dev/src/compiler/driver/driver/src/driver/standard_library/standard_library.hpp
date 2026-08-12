/**
 * @file standard_library.hpp
 * @brief This file declares functions related to handling the standard library in the Duckling
 * compiler meant for outside of the driver users.
 */
#pragma once

#include <driver/task/task.hpp>

namespace compiler::driver {
	/**
	 * @brief Returns the compilation tasks needed to compile the standard library packages.
	 * Returns an empty vector if the compiler is configured to run without stdlib packages,
	 * or if a custom std artifacts directory already contains all the required packages.
	 */
	std::vector<PackageCompilationTask> getRequiredStdLibCompilationTasks();
}
