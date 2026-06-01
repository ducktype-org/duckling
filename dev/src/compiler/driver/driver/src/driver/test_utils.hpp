#pragma once

#include "initialize.hpp"

#include <frontend/module_tree/module_id.hpp>

#include <filesystem/file_path.hpp>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace compiler::driver::test_utils {
	using PackagePathAndName = std::pair<fs::FilePath, std::string>;

	/**
	 * @brief Initializes the compiler for tests with the default standard library enabled.
	 * @param packages Pairs of (package_path, package_name) for test packages.
	 * @param artifacts_path Path where compilation artifacts should be stored.
	 */
	base::CheckedOkBad initializeCompilerForTests(
		const std::vector<PackagePathAndName>& packages, const fs::FilePath& artifacts_path
	);

	/**
	 * @brief Initializes the compiler for tests with custom standard library options.
	 * @param packages Pairs of (package_path, package_name) for test packages.
	 * @param artifacts_path Path where compilation artifacts should be stored.
	 * @param stdlib_options Options describing which stdlib to use.
	 */
	base::CheckedOkBad initializeCompilerForTests(
		const std::vector<PackagePathAndName>& packages,
		const fs::FilePath&                    artifacts_path,
		const options_types::StdLibOptions&    stdlib_options
	);

	/**
	 * @brief Returns module ID for a module path of the form "package/submodule/...".
	 */
	compiler::frontend::ModuleID getModuleIdFromPath(std::string_view module_path);
}
