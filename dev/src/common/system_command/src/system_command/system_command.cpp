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

	i32 SystemCommand::execute(bool error_on_exit_code) {
		std::string out = program_name;
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

		if (error_on_exit_code && exit_code != 0)
			CORE_PANIC(base::strConcat("Command ", out, " exited with code ", exit_code));

		return exit_code;
	}
}
