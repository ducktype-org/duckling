// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "exec_self.hpp"

#include <base/config/target_info.hpp>

#include <cerrno>

#if BASE_TARGET_OS_WINDOWS
	#include <process.h>
#else
	#include <unistd.h>
#endif

namespace os_utils {

	ExecSelfResult execSelf(const std::vector<std::string>& g_argv) {
		std::vector<const char*> args;
		args.reserve(g_argv.size() + 1);
		for (const auto& arg: g_argv) args.push_back(arg.c_str());
		args.push_back(nullptr);

#if BASE_TARGET_OS_WINDOWS
		intptr_t result = _spawnvp(_P_WAIT, args[0], args.data());
		if (result == -1) {
			std::perror("_spawnvp");
			return ExecSelfResult{
				.status     = ExecSelfStatus::Error,
				.error_code = errno,
			};
		}
		return ExecSelfResult{
			.status    = ExecSelfStatus::Spawned,
			.exit_code = static_cast<int>(result),
		};
#else
		// SAFETY: https://pubs.opengroup.org/onlinepubs/9799919799/functions/exec.html, section
		// “Rationale” about constants (look for “The statement about argv[] and envp[] being
		// constants”).
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
		execvp(args[0], const_cast<char* const*>(args.data()));
		std::perror("execvp");
		return ExecSelfResult{
			.status     = ExecSelfStatus::Error,
			.error_code = errno,
		};
#endif
	}

}
