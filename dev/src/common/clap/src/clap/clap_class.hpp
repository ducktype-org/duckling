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
	// TODOP: A simple wrapper for command, which provides the clap functionality.

	class ParsingState;  // Forward declaration.
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

		// Adds a positional argument.
		Clap&& addPositional(Box<ValueParser> parser);

		/**
		 * Adds a subcommand to Clap.
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
		 * TODOP: Docs
		 */
		[[nodiscard]]
		const std::string& getName() const;

		/**
		 * TODOP: Docs
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
		 * TODOP: Docs
		 */
		[[nodiscard]]
		const PreHandler& getPreHandler() const;

		/**
		 * TODOP: Docs
		 */
		[[nodiscard]]
		const Handler& getHandler() const;

		/**
		 * Named parameters are built with clap::ParamBuilder. They are addressed with
		 * ``-${SHORT_NAME}`` or ``--${LONG_NAME}``.
		 * @return A list of named parameters.
		 */
		[[nodiscard]]
		const std::vector<Parameter>& getParameters() const;

		/**
		 * TODOP: Docs
		 */
		[[nodiscard]]
		const std::vector<Box<ValueParser>>& getPositionalParameters() const;

		/**
		 * Returns a list of subcommands for this command.
		 * @return A list of subcommands for this command.
		 */
		[[nodiscard]]
		const std::vector<Clap>& getSubcommands() const;

		/**
		 * TODOP: Docs
		 */
		[[nodiscard]]
		base::Optional<CRef<Clap>> getSubcommand(const std::string& subcommand_name) const;

		/**
		 * TODOP: Docs
		 */
		[[nodiscard]] bool isLeaf() const;

		/**
		 * TODOP: Docs
		 */
		[[nodiscard]] bool isRoot() const;

		/**
		 * Performs the parsing. Returns the parsing result and the matched command.
		 * @param argc Number of elements in argv.
		 * @param argv A C-string array.
		 * @return A pair of the command that matched and the parsed parameters.
		 */
		ParsingResult parse(CLIArgs args);
		/**
		 * TODOP: Docs
		 */
		ParsingResult parse(usize argc, const char* const* argv);
		// ParsingResult parse(const std::string& args); // TODOP: implement that.

		/**
		 * Performs the parsing and executes the command.
		 * @param argc Number of elements in argv.
		 * @param argv A C-string array.
		 * @return A return value od a handler specified for the picked command.
		 */
		int execute(usize argc, const char* const* argv);
		/**
		 * TODOP: Docs
		 */
		// int execute(const std::string& args); // TODOP: Implement that.

	private:
		/**
		 * TODOP: Docs
		 */
		void parse(ParsingState& state) const;

		/**
		 * Validates the result accordingly to the Clap's specification, invokes
		 * conditionals' conditions, etc.
		 * @param result ParsingResult which holds the parsed data.
		 */
		void validateParsing(ParsingResult& result) const;


		/**
		 * TODOP: Docs
		 */
		std::string name;
		std::string description;
		// Is it a leaf in the tree. Has no subcommands.
		bool              is_leaf;
		bool              is_root;
		MBox<ValueParser> default_value_parser;
		PreHandler        pre_handler{};
		Handler           handler{};

		std::vector<Parameter>        parameters;
		std::vector<Box<ValueParser>> positional_parameters;
		std::vector<Clap>             subcommands;
	};
}  // clap
