// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "function_source_names.hpp"

#include <debug_info/debug_info_io.hpp>

#include <base/str/str_utils.hpp>

#include <fstream>
#include <string_view>

namespace vm::api {
	void addFunctionSourceNames(FunctionSourceNames& names, const debug_info::DebugInfo& info) {
		for (const auto& [mangled_name, metadata]: info.functions) {
			if_opt_some(metadata.function_name, source_name) {
				names.put(base::StrID(mangled_name), source_name);
			}
		}
	}

	std::expected<void, std::string> addFunctionSourceNamesFromFile(
		FunctionSourceNames& names, const fs::FilePath& path
	) {
		std::ifstream input(path.getPath(), std::ios::binary);
		if (!input.is_open())
			return std::unexpected(base::strConcat("Cannot open `", path.strView(), "`"));

		auto debug_info = debug_info::loadFromStream(input);
		if (!debug_info.has_value())
			return std::unexpected(
				base::strConcat("Cannot read `", path.strView(), "`: ", debug_info.error())
			);

		addFunctionSourceNames(names, debug_info.value());
		return {};
	}

	base::Optional<fs::FilePath> findDebugInfoNextToBytecode(const std::string& dbc_path) {
		constexpr std::string_view DBC_EXTENSION = ".dbc";
		constexpr std::string_view DI_EXTENSION  = ".di.json";

		if (!dbc_path.ends_with(DBC_EXTENSION)) return {};

		std::string stem = dbc_path;
		stem.resize(stem.size() - DBC_EXTENSION.size());

		fs::FilePath debug_info_path = stem + std::string(DI_EXTENSION);
		if (!debug_info_path.isRegularFile()) return {};

		return debug_info_path;
	}
}
