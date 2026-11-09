// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "function_source_info.hpp"

#include <debug_info/debug_info_io.hpp>

#include <base/str/str_utils.hpp>

#include <algorithm>
#include <fstream>
#include <string_view>
#include <variant>

namespace vm::api {
	namespace {
		/**
		 * @brief The line of a debug info position, if it was resolved to a file and line.
		 */
		base::Optional<SourceLine> toSourceLine(const debug_info::SourcePosition& position) {
			const auto* file_position
				= std::get_if<debug_info::FilePosition>(&position.line_col_position);
			if (file_position == nullptr) return {};
			return SourceLine{ .file = base::StrID(file_position->file_path),
				               .line = file_position->start_line };
		}
	}

	base::Optional<SourceLine> FunctionSourceInfo::lineOfInstruction(u64 instruction_index) const {
		if (instruction_lines.empty()) return {};

		// The first entry after the instruction.
		auto it = std::ranges::upper_bound(
			instruction_lines, instruction_index, {}, &std::pair<u64, SourceLine>::first
		);
		// An instruction without an entry belongs to the last positioned one before it, e.g. the
		// `call` of a call expression comes after the argument setup that carries its line.
		if (it != instruction_lines.begin()) return std::prev(it)->second;
		// Before the first entry, only the first one is left.
		return it->second;
	}

	void addFunctionSourceInfo(FunctionSourceInfos& infos, const debug_info::DebugInfo& info) {
		for (const auto& [mangled_name, metadata]: info.functions) {
			// `put` keeps an existing entry, and creates an empty one otherwise.
			auto& function = infos.put(base::StrID(mangled_name)).first->second;

			if (!function.name.has_value()) function.name = metadata.function_name;

			if (!function.definition.has_value()) {
				if_opt_some(metadata.position, position) {
					function.definition = toSourceLine(position);
				}
			}

			if (function.instruction_lines.empty()) {
				for (const auto& [instruction_index, instruction]:
				     metadata.instr_offsets_to_metadata)
					if_opt_some(toSourceLine(instruction.position), line) {
						function.instruction_lines.emplace_back(instruction_index, line);
					}
				// The debug info file format keeps them sorted, but a hand-built one may not.
				std::ranges::sort(
					function.instruction_lines, {}, &std::pair<u64, SourceLine>::first
				);
			}
		}
	}

	std::expected<void, std::string> addFunctionSourceInfoFromFile(
		FunctionSourceInfos& infos, const fs::FilePath& path
	) {
		std::ifstream input(path.getPath(), std::ios::binary);
		if (!input.is_open())
			return std::unexpected(base::strConcat("Cannot open `", path.strView(), "`"));

		auto debug_info = debug_info::loadFromStream(input);
		if (!debug_info.has_value())
			return std::unexpected(
				base::strConcat("Cannot read `", path.strView(), "`: ", debug_info.error())
			);

		addFunctionSourceInfo(infos, debug_info.value());
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
