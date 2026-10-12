// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "system_command.hpp"

#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>

#include <logger/logger.hpp>

#include <iostream>

namespace system_command {

	SystemCommand& SystemCommand::addArg(std::string arg) {
		arguments.push_back(std::move(arg));
		return *this;
	}

	SystemCommand& SystemCommand::addEnv(std::string name, std::string value) {
		environment.emplace_back(std::move(name), std::move(value));
		return *this;
	}

	i32 SystemCommand::execute(ExitCodeHandling on_exit_code) {
		std::string out;

		for (const auto& [name, value]: environment) {
			out += name;
			out += "=";
			out += value;
			out += " ";
		}

		out += program_name;
		out += " ";

		for (const auto& arg: arguments) {
			out += arg;
			out += " ";
		}

		CORE_DEV_LOG(Command, "[CMD]", out, "\n");

		std::cerr.flush();
		std::cout.flush();

		// Only on POSIX systems
		i32 exit_code = std::system(out.c_str());  // NOLINT(concurrency-mt-unsafe)
		if (WIFSIGNALED(exit_code)) {
			CORE_PANIC(
				base::strConcat("Command ", out, " was terminated by signal ", WTERMSIG(exit_code))
			);
		}
		exit_code = WEXITSTATUS(exit_code);

		if (exit_code != 0) {
			switch (on_exit_code) {
			case ExitCodeHandling::Panic:
				CORE_PANIC(base::strConcat("Command ", out, " exited with code ", exit_code));
			case ExitCodeHandling::Warn:
				CORE_USER_LOG("Warning: Command ", out, " exited with code ", exit_code, "\n");
				break;
			case ExitCodeHandling::Ignore:
				break;
			}
		}

		return exit_code;
	}
}
