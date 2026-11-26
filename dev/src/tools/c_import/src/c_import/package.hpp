// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file package.hpp
 * @brief Layout of a generated package on disk, and the parts of its manifest duckbind writes.
 */
#pragma once

#include "resolve.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace c_import {

	/// Directory under `src/` that `duckbind regen` owns.
	inline constexpr std::string_view GENERATED_DIR = "generated";

	struct PackagePaths final {
		std::filesystem::path root;

		[[nodiscard]] std::filesystem::path manifest() const { return root / "quackconfig.yaml"; }

		[[nodiscard]] std::filesystem::path recipe() const { return root / RECIPE_FILE; }

		[[nodiscard]] std::filesystem::path src() const { return root / "src"; }

		[[nodiscard]] std::filesystem::path generated() const { return src() / GENERATED_DIR; }
	};

	struct ManifestLibraries final {
		std::vector<std::string> external_libs;
		std::vector<std::string> external_lib_paths;
	};

	/// `metadata.name` of a manifest.
	std::expected<std::string, std::string> manifestName(std::string_view manifest);

	/// `metadata.external-libs` and `metadata.external-lib-paths` of a manifest.
	std::expected<ManifestLibraries, std::string> manifestLibraries(std::string_view manifest);

	/// The manifest of a new package.
	std::string newManifest(
		std::string_view name, std::string_view version, const Resolved& resolved
	);

	/**
	 * @brief `manifest` with `metadata.external-libs` and `metadata.external-lib-paths` set to
	 * `resolved`'s, everything else (comments included) kept.
	 * @note Only the one-line form duckbind writes (`  external-libs: [a, b]`) is replaced; an
	 * error names the values to set by hand otherwise.
	 */
	std::expected<std::string, std::string> withLibraryKeys(
		std::string_view manifest, const Resolved& resolved
	);

	/// `src/src.dk`, re-exporting the bindings; written once, then the user's.
	std::string rootModule(std::string_view name);

	/// The main module of `src/generated/`, without which the directory is not a module.
	std::string generatedRootModule(const std::vector<std::string>& banner);

	std::expected<std::string, std::string> readFile(const std::filesystem::path& path);

	/// Writes `content` to `path`, creating its directory.
	std::expected<void, std::string> writeFile(
		const std::filesystem::path& path, std::string_view content
	);

}
