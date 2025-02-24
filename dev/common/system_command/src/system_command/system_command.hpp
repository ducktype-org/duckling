/**
 * @file system_command.hpp
 * @brief Contains SystemCommand class.
 */
#pragma once

#include <base/string_id.hpp>

namespace system_command {

	/**
	 * @brief Builder class for running system commands.
	 *
	 * Example usage:
	 * @code
	 * SystemCommand("ls").addArg("src").addArg("-l").execute();
	 * @endcode
	 *
	 * @warning This class is not thread safe.
	 *
	 * @todo Add support for getting the output of the command.
	 * @todo Add proper error handling.
	 */
	class SystemCommand {
	private:
		base::StrID              program_name;
		std::vector<base::StrID> arguments;

	public:
		SystemCommand(base::StrID program_name): program_name(program_name) {}

		/**
		 * @brief Adds an argument to the command.
		 *
		 * @param arg
		 */
		SystemCommand& addArg(base::StrID arg);

		/**
		 * @brief Executes the command.
		 * @warning Is not thread safe.
		 *
		 * @param echo if true, the command will be echoed to stderr.
		 * @param error_on_exit_code if true, the command will panic if the exit code is not 0.
		 * @return i32 exit code of the command.
		 */
		i32 execute(bool echo = true, bool error_on_exit_code = true);

		~SystemCommand() = default;
	};
}
