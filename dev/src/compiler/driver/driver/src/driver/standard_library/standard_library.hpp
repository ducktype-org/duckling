/**
 * @file standard_library.hpp
 * @brief This file declares functions related to handling the standard library in the Duckling
 * compiler meant for outside of the driver users.
 */
#pragma once

#include <driver/task/task.hpp>

namespace compiler::driver {
	/**
	 * @brief Returns the compilation tasks that would compile the standard library
	 * packages. Can return empty vector if the compiled is configured to run without
	 * stdlib packages. Return only those packages, that are not already compiled when
	 * using
	 */
	std::vector<PackageCompilationTask> getRequiredStdLibCompilationTasks();
}
