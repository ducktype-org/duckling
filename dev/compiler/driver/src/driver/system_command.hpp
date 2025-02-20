#pragma once

#include "base/exceptions.hpp"
#include "base/ints.hpp"
#include "base/str_utils.hpp"
#include "base/string_id.hpp"
#include <iostream>

namespace compiler::driver {

	class SystemCommand {
	private:
		base::StrID              program_name;
		std::vector<base::StrID> arguments;

	public:
		SystemCommand(base::StrID program_name): program_name(program_name) {}

		SystemCommand& addArg(base::StrID arg) {
			arguments.push_back(arg);
			return *this;
		}

		i32 execute(bool echo = true, bool error_on_exit_code = true) {
			std::string out = program_name.str();
			out += " ";

			for (const auto& arg: arguments) {
				out += arg.strView();
				out += " ";
			}
			if (echo) std::cerr << out << "\n";

			std::cout.flush();

			// Only on POSIX systems
			// clang-tidy: disable concurrency-mt-unsafe
			i32 exit_code = std::system(out.c_str());
			if (WIFSIGNALED(exit_code)) {
				CORE_PANIC(base::strConcat(
					"Command ", out, " was terminated by signal ", WTERMSIG(exit_code)
				));
			}
			exit_code = WEXITSTATUS(exit_code);

			if (error_on_exit_code && exit_code != 0) {
				CORE_PANIC(base::strConcat("Command ", out, " exited with code ", exit_code));
			}
			
			return exit_code;
		}

		~SystemCommand() = default;
	};
}
