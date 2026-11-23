// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file recipe.hpp
 * @brief `duckbind.yaml`: how the bindings of a C package are generated.
 *
 * The recipe is published with the package, so its paths are relative to the file, and
 * libraries are named rather than located.
 */
#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace c_import {

	inline constexpr std::string_view RECIPE_FILE = "duckbind.yaml";

	struct Recipe final {
		/// Headers to translate: a path, or a name found on the include path (`SDL3/SDL.h`).
		std::vector<std::string> headers;
		/// pkg-config packages providing compiler flags and library names.
		std::vector<std::string> pkg_config;
		/// Extra compiler flags, such as `-I` and `-D`.
		std::vector<std::string> cflags;
		/// `-l<name>`, `-L<dir>`, or a path to a `lib<name>.a`, `.so` or `.dylib`.
		std::vector<std::string> libraries;
		/// Name globs a declaration has to match to be translated; empty means all.
		std::vector<std::string> include;
		/// Name globs of declarations to leave out.
		std::vector<std::string> exclude;

		bool operator==(const Recipe&) const = default;
	};

	std::expected<Recipe, std::string> parseRecipe(std::string_view yaml);

	/// The recipe as the file's text, with a header comment.
	std::string recipeText(const Recipe& recipe);

	/**
	 * @brief Rewrites the paths given on the command line, relative to `cwd`, as paths relative to
	 * the package's `root`. Header names that are not files (`SDL3/SDL.h`) and `-l` flags are kept.
	 */
	void relativize(
		Recipe& recipe, const std::filesystem::path& cwd, const std::filesystem::path& root
	);

	/**
	 * @brief `path` relative to `base`, with `/` separators. Both are made absolute and
	 * normalized first, so it also works for files that do not exist yet.
	 */
	std::string relativePath(const std::filesystem::path& path, const std::filesystem::path& base);

}
