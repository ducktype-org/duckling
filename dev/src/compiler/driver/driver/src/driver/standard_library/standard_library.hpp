/**
 * @file standard_library.hpp
 * @author Wojciech Rzepliński
 * @brief This file declares functions related to handling the standard library in the Duckling
 * compiler.
 */
#pragma once

#include <driver/options.hpp>
#include <driver/task/task.hpp>
#include <frontend/packages/packages.hpp>

#include "base/types/ok_bad.hpp"
#include <base/collections/optional.hpp>

#include <filesystem/file_path.hpp>

namespace compiler::driver {
	/**
	 * @brief Based on the `GlobalLinkingOptions` returns the path to the standard library, if it is
	 * used. If `DefaultStd` is used, it resolves the path to the standard library based on the
	 * executable path or if `STD_FIXED_PATH` is defined it uses that path.
	 */
	base::Optional<fs::FilePath> resolveStdPath(
		const options_types::GlobalLinkingOptions& standard_library_options
	);

	/**
	 * @brief Creates packages for the standard library and adds them to the global state.
	 * @param std_path Path to the standard library.
	 * @param report Diagnostic reporter to report any issues with the standard library packages
	 * (like a missing package).
	 */
	base::OkBad addStandardLibraryPackages(
		const fs::FilePath& std_path, frontend::packages::DiagnosticReporter& report
	);

	/**
	 * @brief To the `packages_info` vector, to each `RawPackageInfo` in it
	 * adds the dependency on all standard library packages. This way this dependency
	 * doesn't have to be provided by the user in the manifest and is added by the compiler.
	 */
	base::OkBad addStandardLibraryDependencies(
		std::vector<compiler::frontend::packages::RawPackageInfo>& packages_info,
		frontend::packages::DiagnosticReporter&                    report
	);

	/**
	 * @brief Returns the compilation tasks that would compile the standard library.
	 */
	std::vector<PackageCompilationTask> getStandardLibraryCompilationTasks();

	/**
	 * @brief Based on the `GlobalLinkingOptions` returns the string with the arguments needed to
	 * link the standard library. Can be empty if the standard library is not used.
	 */
	base::Optional<std::string> getStdLibLinkingArgs(
		const options_types::GlobalLinkingOptions& linking_options
	);

	/**
	 * @brief The place where the compiled standard library binaries are placed.
	 */
	fs::FilePath getStdBinariesDirectory();
}
