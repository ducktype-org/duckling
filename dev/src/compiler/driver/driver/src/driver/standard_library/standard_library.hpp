/**
 * @file standard_library.hpp
 * @brief This file declares functions related to handling the standard library in the Duckling
 * compiler meant for outside of the driver users.
 */
#pragma once

#include <driver/task/task.hpp>

namespace compiler::driver {
	/**
	 * @brief Returns the compilation tasks that would compile the standard library.
	 */
	std::vector<PackageCompilationTask> getStandardLibraryCompilationTasks();
}
