// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

// @TODO: #3746 cpp-linter fails on Windows (I think because it runs on Ubuntu).
// NOLINTBEGIN
#include <base/config/target_info.hpp>
#if !BASE_TARGET_OS_WINDOWS
	#error "this file is Windows specific"
#endif
#include "system_command.hpp"

#include <process.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/wait.h>

#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>

#include <logger/logger.hpp>
#include <os_utils/thread_safe_wrappers.hpp>

#include <cstddef>
#include <cstring>
#include <iostream>
#include <string_view>
#include <system_error>
using os_rc_t = intptr_t;

using std::literals::operator""sv;

enum class WhyHandleError {
	SpawnFailed,
};

static std::string_view whyHandleErrorToString(WhyHandleError why) {
	switch (why) {
	case WhyHandleError::SpawnFailed:
		return "failed to spawn a subcommand"sv;
	}
	CORE_UNREACHABLE();
}

static void handleRc(
	system_command::SystemCommand::ExitCodeHandling behaviour,
	os_rc_t                                         rc,
	WhyHandleError                                  why,
	std::string_view                                command
);

static void behaviourHandleMessage(
	system_command::SystemCommand::ExitCodeHandling behaviour, std::string_view message
) {
	switch (behaviour) {
	case system_command::SystemCommand::ExitCodeHandling::Panic:
		CORE_PANIC(message, "\n"sv);
		break;
	case system_command::SystemCommand::ExitCodeHandling::Ignore:
		return;
		break;
	case system_command::SystemCommand::ExitCodeHandling::Warn:
		CORE_USER_LOG("Warning: "sv, message, "\n"sv);
		break;
	default:
		CORE_UNREACHABLE();
	}
}

static void handleRc(
	system_command::SystemCommand::ExitCodeHandling behaviour,
	os_rc_t                                         rc,
	WhyHandleError                                  why,
	std::string_view                                command
) {
	CORE_ASSERT(why == WhyHandleError::SpawnFailed, "Windows doesn't have fork&exec");
	if (rc == 0) return;
	// -1 means that we failed to spawn a process.
	if (rc == -1) {
		const auto saved_errno = errno;
		auto       message     = base::strConcat(
            whyHandleErrorToString(why),
            "; command: `"sv,
            command,
            "`: "sv,
            std::generic_category().message(saved_errno)
        );
		behaviourHandleMessage(behaviour, message);
		return;
	}

	auto message = base::strConcat(
		whyHandleErrorToString(why), ": "sv, "command exited with a non-zero code: "sv, rc
	);
	behaviourHandleMessage(behaviour, message);
	return;
}

namespace system_command {

	char** SystemCommand::getOsEnvironPointer() { return _environ; }

	i32 SystemCommand::execute(ExitCodeHandling on_exit_code) const {
		// @TODO: #3746 Use `CommandLineToArgvW`: we have to quote args manually on windows.
		const auto args = this->createArgv();

		const auto args_cstyle
			= SystemCommand::convertToCStyle(args, SystemCommand::AppendNullptr::Yes);

		const auto command_display = this->escapedDisplay();

		std::cerr.flush();
		std::cout.flush();

		// Fast-path: on Windows we can pass `nullptr` to reuse parent process's environment.
		if (this->environment.empty()) {
			intptr_t result = _spawnvpe(_P_WAIT, args_cstyle[0], args_cstyle.data(), nullptr);
			handleRc(on_exit_code, result, WhyHandleError::SpawnFailed, command_display);

			return static_cast<int>(result);
		}
		const auto environment = this->createEnvpWithEnviron();
		const auto env_cstyle
			= SystemCommand::convertToCStyle(environment, SystemCommand::AppendNullptr::Yes);
		intptr_t result = _spawnvpe(_P_WAIT, args_cstyle[0], args_cstyle.data(), env_cstyle.data());
		handleRc(on_exit_code, result, WhyHandleError::SpawnFailed, command_display);

		return static_cast<int>(result);
	}
}
// NOLINTEND
