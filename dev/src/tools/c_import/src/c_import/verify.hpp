// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file verify.hpp
 * @brief Checks a generated package by building and running a program against it with duck.
 */
#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace c_import {

	struct VerifyOptions final {
		std::filesystem::path package_root;
		std::string           name;
		/// The generated layout check module, defining `c_layout_mismatch() -> i64`.
		std::string layout_check;
		/// The checked classes, in the order `c_layout_mismatch` numbers them from 1.
		std::vector<std::string> layouts;
		/// The `duck` to build with.
		std::string duck   = "duck";
		bool        native = true;
		bool        dvm    = false;
	};

	/**
	 * @brief Builds a program depending on the package and runs the layout check on each chosen
	 * backend: it compiles every declaration, links against the library, and compares every class
	 * layout with the one clang reported.
	 */
	std::expected<void, std::string> verify(
		const VerifyOptions& options, const std::function<void(const std::string&)>& info
	);

}
