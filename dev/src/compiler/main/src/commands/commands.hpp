#pragma once

#include <string>
#include <functional>
#include <clap/clap.hpp>

/**
 * @brief Type of command callback. The returned int value is the value
 * that will be returned by hole application (i.e. exit status).
 */
using CommandRunner = std::function<int()>;

/**
 * @brief Structure representing a single command of "duck main"
 */
struct Command final {
	std::string   name;
	std::string   description;
	CommandRunner runner;
};

/**
 * @brief Structure representing all commands of "duck main"
 */
struct CommandList final {
	std::vector<Command> commands;

	/**
	 * @brief Adds new command to the list.
	 */
	void add(std::string name, std::string desc, CommandRunner runner);

	/**
	 * @brief Generate help messages with list of all commands
	 */
	[[nodiscard]]
	std::string generateHelpMessage() const;


	struct CommandStatus final {
		bool was_command_run;
		int  exit_code;
	};

	/**
	 * @brief Runs a command.
	 * @param what Command to run.
	 * @return Whether the command was run.
	 */
	CommandStatus run(std::string_view what);
};



/**
 * @brief Generated command list filled with duck-main commands.
 *
 * @param command_args
 * @param clap Reference to the clap instance that commands wil use (its lifetime must be longer than that of the command list).
 * @return CommandList
 */
CommandList getCommandList(clap::CLIArgs& command_args, clap::Clap& clap);

