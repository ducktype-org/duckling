// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <debug_info/debug_info.hpp>

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>

#include <filesystem/file_path.hpp>
#include <string_id/string_id.hpp>

#include <expected>
#include <string>

namespace vm::api {
	/**
	 * @brief Maps the name of a function in the bytecode (its mangled name) to the name the
	 * function has in the Duck source. The VM uses it to print readable names, e.g. in the stack
	 * trace of a panic.
	 */
	using FunctionSourceNames = base::Map<base::StrID, std::string>;

	/**
	 * @brief Adds the source name of every function in `info` that has one. A function that
	 * already has a name in `names` keeps it.
	 */
	void addFunctionSourceNames(FunctionSourceNames& names, const debug_info::DebugInfo& info);

	/**
	 * @brief Reads the debug info file (`.di.json`) at `path` and adds the source names of its
	 * functions, like `addFunctionSourceNames`.
	 * @return An error message when the file cannot be opened or parsed.
	 */
	std::expected<void, std::string> addFunctionSourceNamesFromFile(
		FunctionSourceNames& names, const fs::FilePath& path
	);

	/**
	 * @brief The debug info file of a bytecode file sits next to it, with the `.dbc` extension
	 * swapped for `.di.json`.
	 * @return The path of the debug info file, if `dbc_path` ends with `.dbc` and the file exists.
	 */
	base::Optional<fs::FilePath> findDebugInfoNextToBytecode(const std::string& dbc_path);
}
