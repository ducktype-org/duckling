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
#include "parsing_state.hpp"
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
	// class ParsingState;  // Forward declaration.
	class Clap final {
	public:
		// TODOP: Signature of a functoin which is run for global configs.
		using PreHandler = std::function<void(clap::ParsingResult&)>;
		using Handler    = std::function<int(const ParsingResult&)>;

		// Clap is move-only
		Clap(std::string name, std::string description = "");
		Clap(Clap& other)                      = delete;
		Clap& operator=(Clap& other) noexcept  = delete;
		Clap(Clap&& other) noexcept            = default;
		Clap& operator=(Clap&& other) noexcept = default;


		/**
		 * Adds a flag/option to the command clap. Ex. --version, --help, --let-it-throw, etc
		 * @param parameter A parameter constructed with clap::ParamBuilder.
		 * @return A reference to self.
		 */
		Clap&& add(Parameter&& param);

		/**
		 * Adds a positional parameter without a name to the Clap.
		 * @param parameter value parser created like: clap::StringParser::make().
		 * @return A reference to self.
		 */
		Clap&& addPositional(Box<ValueParser> parser);

		/**
		 * Adds a subcommand to this command.
		 * @param sub_command A created sub command
		 * @return A reference to self.
		 */
		Clap&& addSubcommand(Clap&& sub_command);

		// Sets a function to be run.
		/**
		 * TODOP: Docs
		 */
		Clap&& setHandler(Handler handler);

		/**
		 * Sets a runner which is run once, before all other handlers.
		 * Usefull when configuring global options like loggers and lexers.
		 * @param pre_handler A function to run.
		 * @return A reference to self.
		 */
		Clap&& setPreHandler(PreHandler pre_handler);

		/**
		 * Sets the default value parser for the whole Clap. Might be a nullptr.
		 * @param parser A value parser to be used.
		 * @return A reference to self.
		 */
		Clap&& setDefaultValueParser(MBox<ValueParser> parser);

		/**
		 * Adds a standard help flag functionality.
		 * If flag is passed raises clap::exceptions::HelpException.
		 * @return A reference to self.
		 */
		Clap&& addHelpFlag();

		/**
		 * @return The name of the command.
		 */
		[[nodiscard]]
		const std::string& getName() const;

		/**
		 * @return The description of the command.
		 */
		[[nodiscard]]
		const std::string& getDescription() const;

		/**
		 * A default value parser is used to parse values, that are not directly specified
		 * in the Clap's specification.
		 * @return A pointer to the parser. Might be nullptr.
		 */
		[[nodiscard]]
		MCRef<ValueParser> getDefaultValueParser() const;

		/**
		 * Returns a handler for this subcommand. Which is a function run when
		 * this command is being invoked.
		 * @return Return the handler function for this command.
		 */
		[[nodiscard]]
		const Handler& getHandler() const;

		/**
		 * @return Return the description of the command.
		 */

		/**
		 * Returns a prehandler for this CLAP. Can only be invoked in the clap command tree root.
		 * Which is a function run before all other handlers.
		 * @return Return the prehandler function for this command.
		 */
		[[nodiscard]]
		const PreHandler& getPreHandler() const;

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
		 * @return A list of subcommands for this command.
		 */
		[[nodiscard]]
		const std::vector<Clap>& getSubcommands() const;

		/**
		 * @return A subcommand with the specified name or and empty optional if the command doesn't
		 * exist.
		 */
		[[nodiscard]]
		base::Optional<CRef<Clap>> getSubcommand(const std::string& subcommand_name) const;

		/**
		 * @return True, if this command is a leaf in the command tree (has no subcommands). False
		 * otherwise. Needed to determine if it's executable.
		 */
		[[nodiscard]] bool isLeaf() const;

		/**
		 * @return True, if this command is a root in the command tree (is not a subcommand).
		 * False otherwise.
		 */
		[[nodiscard]] bool isRoot() const;

		/**
		 * Performs the parsing. Returns the parsing result with the matched command.
		 * @param argc Number of elements in argv.
		 * @param argv A C-string array.
		 * @return An object containing parsed command-line arguments and the matched command.
		 */
		ParsingResult parse(usize argc, const char* const* argv);
		ParsingResult parse(CLIArgs args);
		/**
		 * TODOP: Docs
		 */
		// ParsingResult parse(const std::string& args); // TODOP: implement that.

		/**
		 * Performs the parsing and immediately executes the handler for the matched command.
		 * @param argc Number of elements in argv.
		 * @param argv A C-string array.
		 * @return A return value of the handler specified for the matched command.
		 */
		int execute(usize argc, const char* const* argv);
		/**
		 * TODOP: Docs
		 */
		// int execute(const std::string& args); // TODOP: Implement that.

	private:
		/**
		 * @brief Internal and recursive parsing function for parsing subcommands.
		 * When parsing, if we stumble on a subcommand name we invoke the parse function for the
		 * subcommand and pass the current parsing state as the argument.
		 * @param state Current parsing state which is filled during execution of that function and
		 * may be passed deeper into the tree if a next subcommand is met.
		 */
		void parse(ParsingState& state) const;

		/**
		 * Validates the result accordingly to the Clap's specification, invokes
		 * conditionals' conditions, etc.
		 * @param result ParsingResult which holds the parsed data.
		 */
		void validateParsing(ParsingResult& result) const;


		std::string name; // Name of the command.
		std::string description; // Description of the command
		MBox<ValueParser> default_value_parser;
		
		PreHandler        pre_handler{};
		Handler           handler{};

		std::vector<Parameter>        parameters;
		std::vector<Box<ValueParser>> positional_parameters;
		std::vector<Clap>             subcommands;
		bool              is_leaf;
		bool              is_root;
	};
}  // clap
