/**
 * @file clap.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once
#include "parsing_result.hpp"
#include "base/ints.hpp"
#include "config_parameter.hpp"
#include "value_parser.hpp"

namespace clap {

	/**
	 * Command-line argument parser
	 *
	 * Clap assumes, that all of the non keyword arguments are at the beginning
	 * : ./prog 1 2 3 -o file.txt
	 * Where `1 2 3` are the non keyword arguments
	 *
	 * To construct a ConfigParameter use the ParamBuilder class.
	 *
	 * ./prog --help=<option_name> display longer description.
	 */
	class Clap {
	public:
		Clap() { default_value_parser = StringParser::make(true, " "); }

		Clap& add(ConfigParameter&& parameter);

		ParsingResult parse(usize argc, char* const argv[]);

		[[nodiscard]]
		const ValueParser* getDefaultValueParser() const;
		[[nodiscard]]
		const std::vector<ConfigParameter>& getParameters() const;

	private:
		bool                          started_keyword_args = false;
		base::unique_ptr<ValueParser> default_value_parser;
		std::vector<ConfigParameter>  parameters;
	};

}  // clap
