// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <string>
#include <vector>

namespace os_utils {
	enum class ExecSelfStatus {
		Spawned,
		Error,
	};

	struct ExecSelfResult final {
		ExecSelfStatus status{};
		int            exit_code  = 0;  // Valid when status is ExecSelfStatus::Spawned.
		int            error_code = 0;  // Valid when status is ExecSelfStatus::Error.
	};

	/**
	 * @brief Relaunches the current executable with the original arguments.
	 *
	 * On POSIX systems, this replaces the current process image.
	 * On Windows systems, this spawns a new process and keeps the original one alive.
	 * @param g_argv The argv used to start the original process.
	 * @return On POSIX, a successful exec call does not return.
	 *         On Windows, returns the exit code of the spawned process.
	 *         On error, returns error details.
	 */
	ExecSelfResult execSelf(const std::vector<std::string>& g_argv);
}
