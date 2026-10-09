// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/config/target_info.hpp>
#if !BASE_TARGET_PLATFORM_POSIX
	#error "this file is POSIX specific"
#endif
#include "system_command.hpp"

#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

// Older POSIXes have to manually extern this:
// https://pubs.opengroup.org/onlinepubs/9699919799/xrat/V4_xbd_chap08.html.
extern char** environ;

#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>

#include <logger/logger.hpp>
#include <os_utils/thread_safe_wrappers.hpp>

#include <cstddef>
#include <cstring>
#include <iostream>
#include <string_view>
#include <system_error>
using os_rc_t = int;

using std::literals::operator""sv;

enum class WhyHandleError {
	ForkFailed,
	ExecFailed,
	WaitFailed,
};

static std::string_view whyHandleErrorToString(WhyHandleError why) {
	switch (why) {
	case WhyHandleError::ForkFailed:
		return "fork failed"sv;
	case WhyHandleError::ExecFailed:
		return "exec failed"sv;
	case WhyHandleError::WaitFailed:
		return "waiting for a child failed"sv;
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
	if (rc == 0) return;

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
	// Waitpid, fork and exec return -1 on error.
	CORE_UNREACHABLE();
}

static int handleWaitpidStatus(
	system_command::SystemCommand::ExitCodeHandling behaviour, int status, std::string_view command
) {
	int rc = status;
	if (WIFSIGNALED(rc)) {
		auto signal  = WTERMSIG(rc);
		auto message = base::strConcat(
			"subcommand `"sv,
			command,
			"` terminated by signal: "sv,
			signal,
			" ("sv,
			os_utils::threadSafeStrsignal(signal),
			")"sv
		);
		behaviourHandleMessage(behaviour, message);
		return -1;
	}
	CORE_ASSERT(
		WIFEXITED(rc), "child neither exited normally nor was terminated by a signal: "sv, command
	);
	rc = WEXITSTATUS(rc);
	if (rc == 0) return 0;

	auto message
		= base::strConcat("subcommand: `"sv, command, "` exited with a non-zero code: "sv, rc);
	behaviourHandleMessage(behaviour, message);
	return rc;
}

namespace system_command {

	char** SystemCommand::getOsEnvironPointer() { return environ; }

	i32 SystemCommand::execute(ExitCodeHandling on_exit_code) const {
		const auto args        = this->createArgv();
		auto       environment = this->createEnvpWithEnviron();

		const auto args_cstyle
			= SystemCommand::convertToCStyle(args, SystemCommand::AppendNullptr::Yes);

		const auto command_display = this->escapedDisplay();

		std::cerr.flush();
		std::cout.flush();

		const auto environment_cstyle
			= SystemCommand::convertToCStyle(environment, SystemCommand::AppendNullptr::Yes);

		pid_t pid = 0;
		// SAFETY: https://pubs.opengroup.org/onlinepubs/9799919799/functions/exec.html, section
		// “Rationale” about constants (look for “The statement about argv[] and envp[] being
		// constants”).
		int rc = posix_spawnp(
			&pid,
			args_cstyle[0],
			nullptr,
			nullptr,
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
			const_cast<char* const*>(args_cstyle.data()),
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
			const_cast<char* const*>(environment_cstyle.data())
		);
		if (rc != 0) {
			errno = rc;
			handleRc(on_exit_code, -1, WhyHandleError::ExecFailed, command_display);
			return -1;
		}
		// Parent.
		int status = 0;
		if (waitpid(pid, &status, 0) == -1) {
			handleRc(on_exit_code, -1, WhyHandleError::WaitFailed, command_display);
			return -1;
		}
		return handleWaitpidStatus(on_exit_code, status, command_display);
	}
}
