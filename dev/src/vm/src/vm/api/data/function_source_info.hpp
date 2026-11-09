// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <debug_info/debug_info.hpp>

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/types/ints.hpp>

#include <filesystem/file_path.hpp>
#include <string_id/string_id.hpp>

#include <expected>
#include <string>
#include <utility>
#include <vector>

namespace vm::api {
	/**
	 * @brief A line in a Duckling source file.
	 */
	struct SourceLine final {
		base::StrID file;
		u64         line;
	};

	/**
	 * @brief What the Duckling source says about one function of the bytecode. The VM uses it to
	 * print readable stack traces when a thread panics.
	 */
	struct FunctionSourceInfo final {
		/// The name the function has in the Duckling source.
		base::Optional<std::string> name;
		/// Where the function is defined.
		base::Optional<SourceLine> definition;
		/// The source line of each instruction that has one, sorted by instruction index.
		std::vector<std::pair<u64, SourceLine>> instruction_lines;

		/**
		 * @brief The source line of the instruction at `instruction_index`, in the code collection.
		 * @details Debug info gives a line only to the first instruction of each positioned
		 * compiler instruction. The ones without a line, like a call, mostly compute a value for
		 * the next positioned one, so an instruction without its own line takes the line of the
		 * first entry after it, or of the last entry when there is none after it.
		 * @return Nothing when the function has no instruction lines.
		 */
		[[nodiscard]] base::Optional<SourceLine> lineOfInstruction(u64 instruction_index) const;
	};

	/**
	 * @brief Maps the name of a function in the bytecode (its mangled name) to what the Duckling
	 * source says about it.
	 */
	using FunctionSourceInfos = base::Map<base::StrID, FunctionSourceInfo>;

	/**
	 * @brief Adds the source name and source lines of every function in `info`. Only positions
	 * resolved to lines are used. A function that already has a name, a definition or instruction
	 * lines in `infos` keeps them; only the missing parts are filled in.
	 */
	void addFunctionSourceInfo(FunctionSourceInfos& infos, const debug_info::DebugInfo& info);

	/**
	 * @brief Reads the debug info file (`.di.json`) at `path` and adds what it says about its
	 * functions, like `addFunctionSourceInfo`.
	 * @return An error message when the file cannot be opened or parsed.
	 */
	std::expected<void, std::string> addFunctionSourceInfoFromFile(
		FunctionSourceInfos& infos, const fs::FilePath& path
	);

	/**
	 * @brief The debug info file of a bytecode file sits next to it, with the `.dbc` extension
	 * swapped for `.di.json`.
	 * @return The path of the debug info file, if `dbc_path` ends with `.dbc` and the file exists.
	 */
	base::Optional<fs::FilePath> findDebugInfoNextToBytecode(const std::string& dbc_path);
}
