// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <driver/options.hpp>
#include <frontend/module_tree/module_id.hpp>

#include <base/types/checked_okbad.hpp>

#include <filesystem/file_path.hpp>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace compiler::driver::test_utils {
	using PackagePathAndName = std::pair<fs::FilePath, std::string>;

	/**
	 * @brief Initializes the compiler for tests with custom standard library options.
	 * @param packages Pairs of (package_path, package_name) for test packages.
	 * @param artifacts_path Path where compilation artifacts should be stored.
	 * @param stdlib_options Options describing which stdlib to use.
	 * @param backend_options Options describing which backend to enable. Defaults to no backend
	 * enabled.
	 */
	base::CheckedOkBad initializeCompilerForTests(
		const std::vector<PackagePathAndName>& packages,
		const fs::FilePath&                    artifacts_path,
		const options_types::StdLibOptions&    stdlib_options
		= { options_types::StdLibOptions::DefaultStd{} },
		const global_state::BackendOptions& backend_options = {}
	);

	/**
	 * @brief Returns module ID for a module path of the form "package/submodule/...".
	 */
	compiler::frontend::ModuleID getModuleIdFromPath(std::string_view module_path);
}
