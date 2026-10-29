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

#include <driver/options.hpp>
#include <driver/task/task.hpp>
#include <frontend/packages/packages.hpp>

#include <base/collections/optional.hpp>
#include <base/types/ok_bad.hpp>

#include <filesystem/file_path.hpp>

#include <vector>

namespace compiler::driver {
	/**
	 * @brief Based on the `StdLibOptions` returns the path to the standard library, if it is
	 * used. If `DefaultStd` is used, it resolves the path to the standard library based on the
	 * executable path or if `STD_FIXED_PATH` is defined it uses that path.
	 */
	base::Optional<fs::FilePath> resolveStdPath(
		const options_types::StdLibOptions& standard_library_options
	);

	/**
	 * @brief Adds the dependencies on all the standard library packages to `package_info`, so
	 * that they do not have to be declared in the manifest. Standard library packages are left
	 * unchanged.
	 * @return Bad if the package already depends on a package named like a standard library
	 * package; the issue is reported to `report`.
	 */
	base::OkBad addDependenciesOnStandardLibraryForPackage(
		frontend::packages::RawPackageInfo&     package_info,
		frontend::packages::DiagnosticReporter& report
	);

	/**
	 * @brief Creates the package infos of the standard library packages located in `std_path`.
	 * @return Empty if a standard library package is missing; the issue is reported to `report`.
	 */
	base::Optional<std::vector<frontend::packages::RawPackageInfo>> getStandardLibraryPackages(
		const fs::FilePath& std_path, frontend::packages::DiagnosticReporter& report
	);

	/**
	 * @brief Returns the compilation tasks needed to compile the standard library packages.
	 * Returns an empty vector if the compiler is configured to run without stdlib packages,
	 * or if a custom std artifacts directory already contains all the required packages.
	 */
	std::vector<PackageCompilationTask> getRequiredStdLibCompilationTasks();
}
