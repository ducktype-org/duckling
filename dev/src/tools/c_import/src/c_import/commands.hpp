// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file commands.hpp
 * @brief The commands of `duckbind`. Each one prints its progress and errors and returns the
 * process exit code.
 */
#pragma once

#include "recipe.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace c_import {

	struct NewOptions final {
		std::filesystem::path      at;
		std::optional<std::string> name;
		/// Paths in it are relative to the current directory.
		Recipe      recipe;
		bool        verify = true;
		std::string duck   = "duck";
	};

	/// `duckbind new`: creates a package binding the recipe's headers.
	int newPackage(NewOptions options);

	/// `duckbind regen`: regenerates the package at `at` from its recipe.
	int regenPackage(const std::filesystem::path& at, bool verify, const std::string& duck);

	/// `duckbind check`: fails when the package at `at` differs from what its recipe generates.
	int checkPackage(const std::filesystem::path& at);

	/**
	 * @brief `duckbind bindings`: writes only the binding modules of `recipe_file` into `out`,
	 * for build scripts: no manifest, no `src.dk`, no verification.
	 */
	int writeBindings(
		const std::filesystem::path&      recipe_file,
		const std::filesystem::path&      out,
		const std::optional<std::string>& name
	);

	/// Why `name` cannot name a package, or empty if it can.
	std::string packageNameError(const std::string& name);

	/// Whether `version` is `x.y.z`, all numbers.
	bool isPlainVersion(std::string_view version);

}
