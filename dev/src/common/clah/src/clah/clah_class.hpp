/**
 * @file clah_class.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 * @brief This is a declaration of a class clah::Clah
 * with some template definitions.
 * @example clah_example_cat.cpp
 */

#pragma once

#include "parameter.hpp"
#include "parsing_result.hpp"
#include "parsing_state.hpp"
#include "value_parser.hpp"

#include <base/types/ints.hpp>

#include <functional>
#include <ranges>
#include <string>
#include <vector>

namespace clah {
	/**
	 * @brief Command-line Argument Handler.
	 *
	 * Use clah::ParamBuilder to construct clah::Parameter.
	 *
	 * This class is the place to specify the command-line input for your program. Clah's
	 * specification consists of:
	 * 	* named parameters and flags (built with clah::ParamBuilder)
	 * 	* positional arguments (Always required, indexed from 0)
	 * 	* extra arguments, if default value parser is not set to nullptr.
	 * 	* handlers, functions to be run when a specific command is invoked.
	 * 	* subcommands, which create a tree like structure.
	 *
	 *
	 * After specifying the above you can perform parsing with parse() method or
	 * use the execute() function to immediately perform the execution via the handlers.
	 *
	 * Refer to the clah docs and clah playground for more complete example.
	 */
	class Clah final {
	public:
		using PreHandler = std::function<void(const ParsingResult&)>;
		using Handler    = std::function<int(const ParsingResult&)>;


		Clah();
		Clah(std::string name, std::string description = "");

		// Clah is move-only
		Clah(Clah& other)                      = delete;
		Clah& operator=(Clah& other) noexcept  = delete;
		Clah(Clah&& other) noexcept            = default;
		Clah& operator=(Clah&& other) noexcept = default;


		/**
		 * @brief Adds a named parameter to the command.
		 * @param parameter A parameter constructed with clah::ParamBuilder.
		 * @return A reference to self.
		 */
		Clah&& add(Parameter&& parameter);

		/**
		 * @brief Adds a range of named parameters to the command.
		 * @param parameters A vector of parameters constructed with clah::ParamBuilder.
		 * @return A reference to self.
		 */
		template<std::ranges::input_range Range>
		Clah&& add(Range&& parameters) {
			static_assert(
				!std::is_lvalue_reference_v<Range>, "add(range) requires an rvalue range (temporary)"
			);
			for (auto& param: parameters) add(std::move(param));
			auto _ = std::forward<Range>(parameters);
			return std::move(*this);
		}

		/**
		 * @brief Adds a positional parameter without a name to the Clah.
		 * @param parameter value parser created like: clah::StringParser::make().
		 * @return A reference to self.
		 */
		Clah&& addPositional(Box<ValueParser> parser);

		/**
		 * @brief Adds a subcommand to this command.
		 * @param sub_command A created sub command
		 * @return A reference to self.
		 */
		Clah&& addSubcommand(Clah&& sub_command);

		/**
		 * Sets a handler function to be run when this command is invoked
		 * @param handler The function to run.
		 * @return A reference to self.
		 */
		Clah&& setHandler(Handler handler);

		/**
		 * @brief Sets a runner which is run once, before all other handlers.
		 * Can only be set in the root command.
		 * Usefull when configuring global options like loggers and lexers.
		 * @param pre_handler The function to run.
		 * @return A reference to self.
		 */
		Clah&& setPreHandler(PreHandler pre_handler);

		/**
		 * @brief Sets the default value parser for the command. Might be a nullptr.
		 * @param parser A value parser to be used.
		 * @return A reference to self.
		 */
		Clah&& setDefaultValueParser(MBox<ValueParser> parser);


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
		 * @brief Returns a default value parser is used to parse values, that are not directly
		 * specified in the Clah's specification.
		 * @return A pointer to the parser. Might be nullptr.
		 */
		[[nodiscard]]
		MCRef<ValueParser> getDefaultValueParser() const;

		/**
		 * @brief Returns a handler for this subcommand. Which is a function run when
		 * this command is being invoked.
		 * @return Return the handler function for this command.
		 */
		[[nodiscard]]
		const Handler& getHandler() const;

		/**
		 * Returns a prehandler for this command.
		 * Can only be invoked in the clah command tree root.
		 * Which is a function run before all other handlers.
		 * @return Return the prehandler function for this command.
		 */
		[[nodiscard]]
		const PreHandler& getPreHandler() const;

		/**
		 * Named parameters are built with clah::ParamBuilder. They are addressed with
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
		const std::vector<Clah>& getSubcommands() const;

		/**
		 * @return A subcommand with the specified name or and empty optional if the command doesn't
		 * exist.
		 */
		[[nodiscard]]
		base::Optional<CRef<Clah>> getSubcommand(const std::string& subcommand_name) const;

		/**
		 * @brief Performs the parsing. Returns the parsing result.
		 *
		 * @note It's assumed that the first argv argument is the program name.
		 * @note Various exceptions like HelpException and invalid arguments exceptions are thrown
		 * by this function nd have to be handled by the user.
		 *
		 * @param argc Number of elements in argv.
		 * @param argv A C-string array.
		 * @return An object containing parsed command-line arguments and the matched command.
		 */
		ParsingResult parse(usize argc, const char* const* argv);

		/**
		 * @brief Performs the parsing on a string of arguments. Program name is expected to NOT
		 * exist. Usefull when using CLAP for other things than command line argument parsing.
		 * For example usage see the `argumentParsingTest()` in `clah_test.hpp`.
		 *
		 * @note Various exceptions like HelpException and invalid arguments exceptions are thrown
		 * by this function nd have to be handled by the user.
		 *
		 * @param args A string containing the command line arguments.
		 * @return An object containing parsed command-line arguments and the matched command.
		 */
		ParsingResult parseArgs(const std::string& args);

		/**
		 * @brief Performs the parsing. Then, if the passed arguments where correct if invokes
		 * the pre handler function (if specified) and then immediately executes the handler for the
		 * matched command.
		 *
		 * @note It's assumed that the first argv argument is the program name.
		 * @note All CLAP exceptions are handled inside the execute function. Nicely formatted
		 * messages are printed and help messages are generated.
		 *
		 * Throws an exception if a handler for the invoked command was not specified.
		 * @param argc Number of elements in argv.
		 * @param argv A C-string array.
		 * @return A return value of the handler specified for the matched command.
		 */
		int execute(usize argc, const char* const* argv);

	private:
		/**
		 * @brief Adds a standard help flag functionality.
		 * If flag is passed raises clah::exceptions::HelpException.
		 * @return A reference to self.
		 */
		Clah&& addHelpFlag();

		/**
		 * @brief Internal and recursive parsing function for parsing subcommands.
		 * When parsing, if we stumble on a subcommand name we invoke the parse function for the
		 * subcommand and pass the current parsing state as the argument.
		 * @param state Current parsing state which is filled during execution of that function and
		 * may be passed deeper into the tree if a next subcommand is met.
		 */
		void parse(ParsingState& state) const;

		/**
		 * Validates the result accordingly to the Clah's specification, invokes
		 * conditionals' conditions, etc.
		 * @param result ParsingResult which holds the parsed data.
		 */
		void validateParsing(ParsingResult& result) const;


		std::string name;         // Name of the command.
		std::string description;  // Description of the command

		/**
		 * @brief A parser used to parse extra arguments. Set to StringParser by default.
		 */
		MBox<ValueParser> default_value_parser;

		/**
		 * @brief A function ran before all other handlers at the moment of execute().
		 * It's ran only if the passed arguments are correct.
		 * Usefull to configure the application state.
		 * It can only be specified in the command tree root, doing otherwise will result in an
		 * exception.
		 */
		PreHandler pre_handler{};

		/**
		 * @brief A function which provides the functionality for a command.
		 * It's being run when the command is invoked.
		 * Has to be specified in order to use execute() on a command. Not doing so will result in
		 * an exception.
		 */
		Handler handler{};

		std::vector<Parameter>        parameters;
		std::vector<Box<ValueParser>> positional_parameters;

		/**
		 * @brief A list of subcommands (sub-clahs) for this command.
		 */
		std::vector<Clah> subcommands;
	};
}  // clah
