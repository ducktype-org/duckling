// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "system_command.hpp"

#include <sys/wait.h>

#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>

#include <logger/logger.hpp>

#include <cstddef>
#include <cstring>
#include <iostream>
#include <string_view>

#if BASE_TARGET_OS_WINDOWS
	#include <process.h>
	#include <stdint.h>
using os_rc_t = intptr_t;
#else
	#include <unistd.h>
using os_rc_t = int;
#endif

using std::literals::operator""sv;

enum class WhyHandleError {
	ForkFailed,
	ExecFailed,
	WaitFailed,
	SubprocessFailed,
};

static std::string_view whyHandleErrorToString(WhyHandleError why) {
	switch (why) {
	case WhyHandleError::ForkFailed:
		return "fork failed"sv;
	case WhyHandleError::ExecFailed:
		return "exec failed"sv;
	case WhyHandleError::SubprocessFailed:
		return "failed to spawn a command"sv;
	case WhyHandleError::WaitFailed:
		return "waiting for a child failed"sv;
	}
	CORE_UNREACHABLE();
}

static int handleRc(
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

#if BASE_TARGET_OS_WINDOWS
static int handleRc(
	system_command::SystemCommand::ExitCodeHandling behaviour,
	os_rc_t                                         rc,
	WhyHandleError                                  why,
	std::string_view                                command
) {
	CORE_ASSERT(why == WhyHandleError::SubprocessFailed, "Windows doesn't have fork&exec");
	if (rc == 0) return rc;
	// -1 means that we failed to spawn a process.
	if (rc == -1) {
		std::array<char, 1'024> buffer{};
		strerror_s(buffer.data(), buffer.size(), errno);
		auto message = base::strConcat(
			whyHandleErrorToString(why), "; command: `"sv, command, "`: "sv, buffer.data()
		);
		behaviourHandleMessage(behaviour, message);
		return rc;
	}

	auto message = base::strConcat(
		whyHandleErrorToString(why), ": "sv, "command exited with a non-zero code: "sv, rc
	);
	behaviourHandleMessage(behaviour, message);
	return rc;
}
#else
static int handleRc(
	system_command::SystemCommand::ExitCodeHandling behaviour,
	os_rc_t                                         rc,
	WhyHandleError                                  why,
	std::string_view                                command
) {
	if (why != WhyHandleError::SubprocessFailed) {
		if (rc == 0) return 0;

		if (rc == -1) {
			std::array<char, 1'024> buffer{};
			strerror_r(errno, buffer.data(), buffer.size());
			auto message = base::strConcat(
				whyHandleErrorToString(why), "; command: `"sv, command, "`: "sv, buffer.data()
			);
			behaviourHandleMessage(behaviour, message);
			return -1;
		}
		// Wait, fork and exec return -1 on error.
		CORE_UNREACHABLE();
	}
	if (WIFSIGNALED(rc)) {
		auto signal  = WTERMSIG(rc);
		auto message = base::strConcat(
			whyHandleErrorToString(why),
			": command `"sv,
			command,
			"` terminated by signal: "sv,
			signal,
			" ("sv,
			// NOLINTNEXTLINE(concurrency-mt-unsafe),
			strsignal(signal),
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

	auto message = base::strConcat(
		whyHandleErrorToString(why),
		": "sv,
		"command: `"sv,
		command,
		"` exited with a non-zero code: "sv,
		rc
	);
	behaviourHandleMessage(behaviour, message);
	return rc;
}
#endif

namespace system_command {

	SystemCommand& SystemCommand::addArg(std::string arg) {
		arguments.push_back(std::move(arg));
		return *this;
	}

	SystemCommand& SystemCommand::addEnv(std::string name, std::string value) {
		environment.emplace_back(std::move(name), std::move(value));
		return *this;
	}

	i32 SystemCommand::execute(ExitCodeHandling on_exit_code) const {
		std::vector<const char*> args;
		// +2 is for program name and nullptr;
		args.reserve(this->arguments.size() + 2);
		args.push_back(this->program_name.data());
		for (const auto& arg: this->arguments) args.push_back(arg.c_str());
		args.push_back(nullptr);

		std::vector<std::string> environment;
		environment.reserve(this->environment.size() + 1);
		for (const auto& env: this->environment)
			environment.push_back(env.first + "=" + env.second);

		std::vector<const char*> environment_cstr;
		environment_cstr.reserve(environment.size());
		for (const auto& env: environment) environment_cstr.push_back(env.c_str());
		environment_cstr.push_back(nullptr);

		std::string command_display{};
		for (const auto& env: this->environment)
			command_display += '"' + env.first + "=" + env.second + "\" ";
		command_display += '"' + this->program_name + '"';
		for (const auto& arg: this->arguments) command_display += " \"" + arg + '"';

		std::cerr.flush();
		std::cout.flush();

#if BASE_TARGET_OS_WINDOWS
		intptr_t result = _spawnvpe(_P_WAIT, args[0], args.data(), environment_cstr.data());
		handleRc(on_exit_code, result, WhyHandleError::SubprocessFailed, command_display);

		return static_cast<int>(result);
#else
		const auto pid = fork();
		if (pid == -1) {
			handleRc(on_exit_code, pid, WhyHandleError::ForkFailed, command_display);
			return -1;
		}
		// Child.
		if (pid == 0) {
			// @TODO: #3734 Change this to `execvpe`.
			for (const auto& env: this->environment) {
				// NOLINTNEXTLINE(concurrency-mt-unsafe),
				setenv(env.first.c_str(), env.second.c_str(), 1);
			}

			// SAFETY: https://pubs.opengroup.org/onlinepubs/9799919799/functions/exec.html, section
			// “Rationale” about constants (look for “The statement about argv[] and envp[] being
			// constants”).
			execvp(
				args[0],
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
				const_cast<char* const*>(args.data())
			);
			// If `execvp` returns, it failed with -1. Force `Warn`, panic may have unreliable exit
			// codes.
			handleRc(ExitCodeHandling::Warn, -1, WhyHandleError::ExecFailed, command_display);
			// Force kill child :).
			_exit(1);
		}
		// Parent.
		int status = 0;
		if (waitpid(pid, &status, 0) == -1) {
			handleRc(on_exit_code, -1, WhyHandleError::WaitFailed, command_display);
			return -1;
		}
		return handleRc(on_exit_code, status, WhyHandleError::SubprocessFailed, command_display);
#endif
	}
}
