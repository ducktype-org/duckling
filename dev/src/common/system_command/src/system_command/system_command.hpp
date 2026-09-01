/**
 * @file system_command.hpp
 * @brief Contains SystemCommand class.
 */
#pragma once

#include <string_id/string_id.hpp>

#include <string>
#include <utility>
#include <vector>

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
	 * @todo Change base::StrID to std::string
	 * @todo Add support for getting the output of the command.
	 * @todo Add proper error handling.
	 */
	class SystemCommand {
	private:
		std::string                                      program_name;
		std::vector<std::string>                         arguments;
		std::vector<std::pair<std::string, std::string>> environment;

	public:
		SystemCommand(std::string program_name): program_name(std::move(program_name)) {}

		/**
		 * @brief Adds an argument to the command.
		 *
		 * @param arg
		 */
		SystemCommand& addArg(std::string arg);

		/**
		 * @brief Adds an environment variable to the command's environment.
		 *
		 * The variable is set for the spawned command only, so this does not touch the
		 * environment of the compiler itself (which several threads may be reading).
		 *
		 * @param name
		 * @param value
		 */
		SystemCommand& addEnv(std::string name, std::string value);

		/**
		 * @brief Helper enum to specify how to handle non-zero exit codes from the command.
		 */
		enum class ExitCodeHandling {
			Panic,
			Ignore,
			Warn,
		};

		/**
		 * @brief Executes the command.
		 * @warning Is not thread safe.
		 *
		 * @return i32 exit code of the command.
		 */
		i32 execute(ExitCodeHandling on_exit_code = ExitCodeHandling::Panic);

		~SystemCommand() = default;
	};
}
