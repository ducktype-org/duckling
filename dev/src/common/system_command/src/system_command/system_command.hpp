// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
		 * @note The string is passed as-is, the caller has to wrap it in the parenthesis
		 * if it contains spaces.
		 *
		 * @param arg
		 */
		SystemCommand& addArg(std::string arg);

		/**
		 * @brief Adds multiple arguments to the command.
		 * @note The strings are passed as-is, the caller has to wrap them in the parenthesis
		 * if they contains spaces.
		 *
		 * @param args
		 */
		SystemCommand& addArgs(const std::vector<std::string>& args) {
			for (const auto& arg: args) this->addArg(arg);
			return *this;
		}

		/**
		 * @brief Adds an environment variable to the command's environment.
		 *
		 * The variable is set for the spawned command only, so this does not touch the
		 * environment of the compiler itself (which several threads may be reading).
		 *
		 * @note The string is passed as-is, the caller has to wrap it in the parenthesis
		 * if it contains spaces.
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
