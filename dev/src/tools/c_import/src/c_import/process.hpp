// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file process.hpp
 * @brief Running other programs and capturing their output, and temporary directories.
 */
#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace c_import {

	struct ProcessOutput final {
		int         exit_code = 0;
		std::string out;
		std::string err;
	};

	/**
	 * @brief Runs `program` (looked up on `PATH`) with `args` in `cwd` (the current directory when
	 * empty) and waits for it.
	 * @return An error when the program cannot be started.
	 */
	std::expected<ProcessOutput, std::string> runProcess(
		const std::string&              program,
		const std::vector<std::string>& args,
		const std::filesystem::path&    cwd = {}
	);

	/// A new directory under the system's temporary directory, removed with everything in it.
	class TempDir final {
	public:
		/// @throws std::filesystem::filesystem_error when the directory cannot be created.
		TempDir();
		TempDir(const TempDir&)            = delete;
		TempDir& operator=(const TempDir&) = delete;
		TempDir(TempDir&&)                 = delete;
		TempDir& operator=(TempDir&&)      = delete;
		~TempDir();

		[[nodiscard]] const std::filesystem::path& path() const { return dir; }

	private:
		std::filesystem::path dir;
	};

}
