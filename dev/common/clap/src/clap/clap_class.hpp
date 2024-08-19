/**
 * @file clap_class.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include "parsing_result.hpp"
#include <base/ints.hpp>
#include "parameter.hpp"
#include "value_parser.hpp"

namespace clap {
	struct CLIArgs {
		usize              argc;  /// Argument count
		const char* const* argv;  /// Pointer to an array of strings
	};

	/**
	 * Command-line Argument Parser.
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
	class Clap {
	public:
		Clap();

		Clap(Clap& other) noexcept: Clap(std::move(other)) {}

		Clap(Clap&& other) noexcept:
			  default_value_parser(std::move(other.default_value_parser)),
			  positional_parameters(std::move(other.positional_parameters)),
			  parameters(std::move(other.parameters)) {}

		/**
		 * Adds a named parameter to the Clap.
		 * @param parameter A parameter constructed with clap::ParamBuilder.
		 * @return A reference to self.
		 */
		Clap& add(Parameter&& parameter);

		/**
		 * Adds a positional parameter without a name to the Clap.
		 * @param parameter value parser created like: clap::StringParser::make().
		 * @return A reference to self.
		 */
		Clap& addPositional(base::unique_ptr<ValueParser> parameter);

		/**
		 * Sets the default value parser for the Clap. Might be a nullptr.
		 * @param parser A value parser to be used.
		 * @return A reference to self.
		 */
		Clap& setDefaultParser(base::unique_ptr<ValueParser> parser);

		/**
		 * Adds a standard help flag functionality.
		 * If flag is passed raises clap::exceptions::HelpException.
		 * @return A reference to self.
		 */
		Clap& addHelpFlag();

		/**
		 * Perform parsing.
		 * @param argc Number of elements in argv.
		 * @param argv A C-string array.
		 * @return An object containing parsed command-line arguments.
		 */
		ParsingResult parse(usize argc, const char* const* argv);
		ParsingResult parse(CLIArgs args);

		/**
		 * A default value parser is used to parse values, that are not directly specified
		 * in the Clap's specification.
		 * @return A pointer to the parser. Might be nullptr.
		 */
		[[nodiscard]]
		base::borrow_ptr<const ValueParser> getDefaultValueParser() const;

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
		const std::vector<base::unique_ptr<ValueParser>>& getPositionalParameters() const;

	private:
		base::unique_ptr<ValueParser>              default_value_parser;
		std::vector<base::unique_ptr<ValueParser>> positional_parameters;
		std::vector<Parameter>                     parameters;

		/**
		 * Validates the result accordingly to the Clap's specification, invokes
		 * conditionals' conditions, etc.
		 * @param result ParsingResult which holds the parsed data.
		 */
		void validateParsing(ParsingResult& result) const;
	};

}  // cla
