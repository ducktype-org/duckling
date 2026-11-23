// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file resolve.hpp
 * @brief Turns a recipe into compiler flags for reading the headers and into the library names
 * written to the package's manifest.
 *
 * Nothing machine-specific goes into the manifest: libraries are written as bare names and duck
 * finds their files when the package is built.
 */
#pragma once

#include "recipe.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace c_import {

	/// A recipe resolved on this machine.
	struct Resolved final {
		/// Flags passed to clang when reading the headers.
		std::vector<std::string> cflags;
		/// `metadata.external-libs`: bare library names.
		std::vector<std::string> external_libs;
		/**
		 * @brief `metadata.external-lib-paths`: directories relative to the package's root, or
		 * absolute directories that duck does not search by default.
		 */
		std::vector<std::string> external_lib_paths;
		/// `(pkg-config package, version)` of every pkg-config package.
		std::vector<std::pair<std::string, std::string>> versions;
		/// Linker flags that were left out, with the reason.
		std::vector<std::string> warnings;
	};

	/**
	 * @brief Resolves `recipe` for the package rooted at `root`: runs pkg-config and maps library
	 * flags and files to names and directories.
	 * @param host_flags Whether to add the flags reading the headers for this machine's target.
	 */
	std::expected<Resolved, std::string> resolve(
		const Recipe& recipe, const std::filesystem::path& root, bool host_flags = true
	);

	/// `foo` for `libfoo.a`, `libfoo.so`, `libfoo.so.1.2` and `libfoo.dylib`; empty otherwise.
	std::string libraryName(std::string_view file_name);

}
