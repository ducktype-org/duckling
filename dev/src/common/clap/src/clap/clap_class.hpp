/**
 * @file clap_class.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 * @brief This is a declaration of a class clap::Clap
 * with some template definitions.
 * @example clap_example_cat.cpp
 */

#pragma once

#include "parameter.hpp"
#include "parsing_result.hpp"
#include "value_parser.hpp"
#include "command.hpp"

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
		using CommandAndArgs = std::pair<const Command&, ParsingResult>;

		Clap();
		Clap(const std::string& name, const std::string& description);

		// Clap is move-only
		Clap& operator=(Clap&& other) noexcept {
			root_command = std::move(other.root_command);
			return *this;
		}

		// Clap is move-only
		Clap(Clap&& other) noexcept: Clap() { *this = std::move(other); }

		// TODOP: MODIFIERS 

		/**
		 * Adds a subcommand to the Clap.
		 * @param parameter A parameter constructed with clap::ParamBuilder.
		 * @return A reference to self.
		 */
		Clap&& addSubcommand(Command&& sub_command);

		/**
		 * Sets a runner which is run once, before all other handlers.
		 * Usefull when configuring global options like loggers and lexers.
		 * @param pre_runner A function to run.
		 * @return A reference to self.
		 */
		Clap&& setPreRunner(PreHandler pre_handler);

		/**
		 * Adds a global flag/option for clap. Ex. version, help, --let-it-throw, etc.
		 * @param parameter A parameter constructed with clap::ParamBuilder.
		 * @return A reference to self.
		 */
		Clap&& addGlobalOption(Parameter&& parameter);

		/**
		 * Sets the default value parser for the Clap. Might be a nullptr.
		 * @param parser A value parser to be used.
		 * @return A reference to self.
		 */
		Clap&& setDefaultParser(MBox<ValueParser> parser);

		/**
		 * Adds a standard help flag functionality.
		 * If flag is passed raises clap::exceptions::HelpException.
		 * @return A reference to self.
		 */
		Clap&& addHelpFlag();

		// TODOP: FUNCTIONALITY.
		/**
		 * Perform parsing.
		 * @param argc Number of elements in argv.
		 * @param argv A C-string array.
		 * @return An object containing parsed command-line arguments.
		 */
		ParsingResult parse(usize argc, const char* const* argv);
		ParsingResult parse(CLIArgs args);
		ParsingResult parse(const std::string& args);
		
		// Main function which chooses the correct command being run, and runs it.
		int execute(int argc, const char* const* argv);


		// TODOP: Getters.
		/**
		 * A default value parser is used to parse values, that are not directly specified
		 * in the Clap's specification.
		 * @return A pointer to the parser. Might be nullptr.
		 */
		[[nodiscard]]
		MCRef<ValueParser> getDefaultValueParser() const;

		/**
		 * Named parameters are built with clap::ParamBuilder. They are addressed with
		 * ``-${SHORT_NAME}`` or ``--${LONG_NAME}``.
		 * @return A list of named parameters.
		 */
		[[nodiscard]]
		const std::vector<Parameter>& getParameters() const;

		/**
		 * Positional parameters are indexed from zero and they are always required.
		 * @return A list of positional parameters (their value parsers).
		 */
		[[nodiscard]]
		const std::vector<Box<ValueParser>>& getPositionalParameters() const;

		/**
		 * Returns a list of subcommands for this command
		 * @return A list of subcommands for this command
		 */
		[[nodiscard]]
		const base::HashMap<base::StrID, MRef<Clap>>& getSubcommands() const;

	private:
		/**
		 * Parses the argv, returns a command to be run and it's parameters.
		 * @param result ParsingResult which holds the parsed data.
		 */
		CommandAndArgs parse(int argc, const char* const* argv);
		
		/**
		 * Validates the result accordingly to the Clap's specification, invokes
		 * conditionals' conditions, etc.
		 * @param result ParsingResult which holds the parsed data.
		 */
		void validateParsing(ParsingResult& result) const;

		Command root_command; // Root command, stores the global options and subcommands.
	};


}  // clap
