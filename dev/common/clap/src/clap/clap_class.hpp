/**
 * @file clap_class.h
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include "parsing_result.hpp"
#include <base/ints.hpp>
#include "parameter.hpp"
#include "value_parser.hpp"
#include <memory>

namespace clap {
	struct CLIArgs {
		usize        argc;  // Argument count
		const char** argv;  // Pointer to an array of strings
	};

	/**
	 * Command-line argument parser.
	 *
	 * Use clap::ParamBuilder to construct clap::Parameter.
	 *
	 * Refer to the clap docs for more complete example.
	 */
	class Clap {
	public:
		Clap();

		Clap(Clap& other) noexcept:
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
		 * Adds a positional parameter without a value to the Clap.
		 * @param parameter Parameter's value parser created like: clap::StringParser::make().
		 * @return A reference to self.
		 */
		Clap& addPositional(base::unique_ptr<ValueParser> parameter);

		/**
		 * Sets the default value parser for the Clap.
		 * @param parser A value parses to be used.
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
		ParsingResult parse(usize argc, const char** argv);
		ParsingResult parse(CLIArgs args);

		[[nodiscard]]
		const ValueParser* getDefaultValueParser() const;
		[[nodiscard]]
		const std::vector<Parameter>& getParameters() const;
		[[nodiscard]]
		const std::vector<base::unique_ptr<ValueParser>>& getPositionalParameters() const;

	private:
		base::unique_ptr<ValueParser>              default_value_parser;
		std::vector<base::unique_ptr<ValueParser>> positional_parameters;
		std::vector<Parameter>                     parameters;

		void validateParsing(ParsingResult& result) const;
	};

}  // cla
