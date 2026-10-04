// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
