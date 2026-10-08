// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file standard_library.hpp
 * @brief The private header for the standard library handling in the Duckling compiler,
 * for the code that is only used inside the driver.
 *
 * The role of these functions is to replicate what the package manager does by
 * adding the packages and dependencies of the standard library manually.
 */
#pragma once

#include <driver/options.hpp>
#include <driver/standard_library/standard_library.hpp>

#include <artifacts/artifacts.hpp>
#include <filesystem/file_path.hpp>

#include <vector>

namespace compiler::driver {
	/**
	 * @brief Based on the `StdLibOptions` returns the string with the arguments needed to
	 * link the standard library. Can be empty if the standard library is not used.
	 */
	std::vector<std::string> getNativeStdLibLinkingArgs(
		const options_types::StdLibOptions& linking_options
	);

	/**
	 * @brief The existing compiled standard library binaries for native targets.
	 */
	std::vector<artifacts::FileArtifact> getStdLibNativeArtifacts();

	/**
	 * @brief Based on the `StdLibOptions` returns the paths of the standard library DVM
	 * artifacts, to be linked as dependencies. Empty if the standard library is not used.
	 */
	std::vector<fs::FilePath> getStdLibDVMLinkingDependencies(
		const options_types::StdLibOptions& standard_library_options
	);

	/**
	 * @brief The existing compiled standard library DVM artifacts.
	 */
	std::vector<artifacts::FileArtifact> getStdLibDVMArtifacts();

	/**
	 * @brief Returns whether all the standard library artifact files are present.
	 * @note It is used to determine if we can skip std compilation.
	 * @TODO: #3158 A generic dependency no-recompile solution may replace this.
	 */
	bool allStdlibArtifactsPresent();
}
