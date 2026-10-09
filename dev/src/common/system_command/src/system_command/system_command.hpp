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

		/**
		 * @brief Create an escaped string for debug logs.
		 */
		[[nodiscard]] std::string escapedDisplay() const;
		/**
		 * @brief Create a vector of program name + arguments.
		 */
		[[nodiscard]] std::vector<std::string> createArgv() const;
		/**
		 * @brief Create a vector of environments; each entry is formatted as `{key}={value}`.
		 */
		[[nodiscard]] std::vector<std::string> createEnvp() const;

		enum class AppendNullptr {
			Yes,
			No,
		};

		/**
		 * @brief Convert a vector of `std::string`s into a vector of `const char*`.
		 * @note Inner `const char *` live as long as `input` strings. You can control whether or
		 * not append `nullptr` via `append_nullptr`.
		 *
		 * @param input
		 * @param append_nullptr
		 */
		[[nodiscard]] static std::vector<const char*> convertToCStyle(
			const std::vector<std::string>& input, AppendNullptr append_nullptr
		);

	public:
		SystemCommand(std::string program_name): program_name(std::move(program_name)) {}

		/**
		 * @brief Adds an argument to the command.
		 * @note The string is appended to the `argv` vector, and passed to the
		 * `execvp`/`_spawnvpe`.
		 *
		 * @param arg
		 */
		SystemCommand& addArg(std::string arg);

		/**
		 * @brief Adds multiple arguments to the command.
		 * @note The strings are appended to the `argv` vector, and passed to the
		 * `execvp`/`_spawnvpe`.
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
		 * The variable should be set for the spawned command only, so this should not touch the
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
		[[nodiscard]] i32 execute(ExitCodeHandling on_exit_code = ExitCodeHandling::Panic) const;

		~SystemCommand() = default;
	};
}
