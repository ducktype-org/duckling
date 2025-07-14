/**
 * @file clap_class.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 * @brief This is a declaration of a class clap::Clap
 * with some template definitions.
 * @example clap_example_cat.cpp
 */

#pragma once

#include "command.hpp"
#include "parameter.hpp"
#include "parsing_result.hpp"
#include "value_parser.hpp"

#include "base/maps.hpp"
#include "base/string_id.hpp"
#include <base/ints.hpp>

#include <functional>
#include <string>
#include <vector>

namespace clap {
	struct CLIArgs final {        // TODOP: That my be not needed anymore.
		usize              argc;  /// Argument count
		const char* const* argv;  /// Pointer to an array of strings
	};

	/**
	 * @brief Command-line Argument Parser.
	 *
	 * Use clap::ParamBuilder to construct clap::Parameter.
	 *
	 * This class is the place to specify the command-line input for your program. Clap's
	 * specification consists of:
	 * 	* named parameters and flags (built with clap::ParamBuilder)
	 * 	* positional arguments (Always required, indexed from 0)
	 * 	* extra arguments, if default value parser is not set to nullptr.
	 *
	 * After specifying the above you can perform parsing with parse() method.
	 *
	 * Refer to the clap docs for more complete example.
	 */
	// TODOP: A simple wrapper for command, which provides the clap functionality.
	class Clap final {
	public:
		using PreHandler = std::function<void(clap::ParsingResult&)>;

		// Clap is move-only
		Clap(std::string name, std::string description = "");
		Clap(Clap& other)                      = delete;
		Clap& operator=(Clap& other) noexcept  = delete;
		Clap(Clap&& other) noexcept            = default;
		Clap& operator=(Clap&& other) noexcept = default;

		// TODOP: MODIFIERS

		/**
		 * Adds a subcommand to Clap.
		 * @param sub_command A created sub command
		 * @return A reference to self.
		 */
		Clap&& addSubcommand(Command&& sub_command);

		/**
		 * Adds a global flag/option for clap. Ex. --version, --help, --let-it-throw, etc
		 * Adds a parameter to the root command.
		 * @param parameter A parameter constructed with clap::ParamBuilder.
		 * @return A reference to self.
		 */
		Clap&& addGlobalParameter(Parameter&& parameter);
		Clap&& addPositional(Box<ValueParser> parser);

		/**
		 * Sets the default value parser for the Clap. Might be a nullptr.
		 * @param parser A value parser to be used.
		 * @return A reference to self.
		 */
		Clap&& setDefaultValueParser(MBox<ValueParser> parser);

		/**
		 * Sets a runner which is run once, before all other handlers.
		 * Usefull when configuring global options like loggers and lexers.
		 * @param pre_handler A function to run.
		 * @return A reference to self.
		 */
		Clap&& setPreHandler(PreHandler pre_handler);

		/**
		 * Adds a standard help flag functionality.
		 * If flag is passed raises clap::exceptions::HelpException.
		 * @return A reference to self.
		 */
		Clap&& addHelpFlag();

		// TODOP: Getters.
		/**
		 * Returns a list of subcommands for this command.
		 * @return A list of subcommands for this command.
		 */
		[[nodiscard]]
		const std::vector<Command>& getSubcommands() const;

		/**
		 * Named parameters are built with clap::ParamBuilder. They are addressed with
		 * ``-${SHORT_NAME}`` or ``--${LONG_NAME}``.
		 * @return A list of named parameters.
		 */
		[[nodiscard]]
		const std::vector<Parameter>& getGlobalParameters() const;

		/**
		 * A default value parser is used to parse values, that are not directly specified
		 * in the Clap's specification.
		 * @return A pointer to the parser. Might be nullptr.
		 */
		[[nodiscard]]
		MCRef<ValueParser> getDefaultValueParser() const;

		/**
		 * A default value parser is used to parse values, that are not directly specified
		 * in the Clap's specification.
		 * @return A pointer to the parser. Might be nullptr.
		 */
		[[nodiscard]]
		const PreHandler& getPreHandler() const;

		[[nodiscard]]
		const Command& getRootCommand() const;


		// TODOP: FUNCTIONALITY.
		/**
		 * Perform parsing.
		 * @param argc Number of elements in argv.
		 * @param argv A C-string array.
		 * @return An object containing parsed command-line arguments.
		 */
		// TODOP: This may be not needed.
		// ParsingResult parse(usize argc, const char* const* argv);
		// ParsingResult parse(CLIArgs args);
		// ParsingResult parse(const std::string& args);

		/**
		 * Performs the parsing. Returns the parsing result and the matched command.
		 * @param argc Number of elements in argv.
		 * @param argv A C-string array.
		 * @return A pair of the command that matched and the parsed parameters.
		 */
		ParsingResult parse(CLIArgs args);
		ParsingResult parse(usize argc, const char* const* argv);

		/**
		 * Performs the parsing and executes the command.
		 * @param argc Number of elements in argv.
		 * @param argv A C-string array.
		 * @return A return value od a handler specified for the picked command.
		 */
		int execute(usize argc, const char* const* argv);

		// Version of the above one, but parses from a string and discards the program name.
		// int execute(const std::string& args); // TODOP: Implement that.

		// TODOP: Remove that.
		void dPrint();

	private:
		/**
		 * Validates the result accordingly to the Clap's specification, invokes
		 * conditionals' conditions, etc.
		 * @param result ParsingResult which holds the parsed data.
		 */
		void validateParsing(ParsingResult& result) const;


		// Root command, stores the global options, subcommands of the global CLAP object.
		Command    root_command;
		PreHandler pre_handler;
	};
}  // clap
