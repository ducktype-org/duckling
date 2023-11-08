/**
 * @file clap.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once
#include "parsing_result.hpp"
#include "base/ints.hpp"
#include "clap_parameter.hpp"
#include "value_parser.hpp"
#include <memory>

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
		Clap();

		Clap(Clap& other) noexcept:
			  default_value_parser(std::move(other.default_value_parser)),
			  positional_parameters(std::move(other.positional_parameters)),
			  parameters(std::move(other.parameters)) {}

		/**
		 * Adds parameter to the Clap object. Positional arguments should come before named
		 * arguments.
		 * @param parameter
		 * @return
		 */
		Clap& add(ClapParameter&& parameter);

		Clap& addPositional(base::unique_ptr<ValueParser> parameter);

		Clap& setDefaultParser(base::unique_ptr<ValueParser> parser);

		Clap& addHelpFlag();

		ParsingResult parse(usize argc, const char** argv);

		[[nodiscard]]
		const ValueParser* getDefaultValueParser() const;
		[[nodiscard]]
		const std::vector<ClapParameter>& getParameters() const;
		[[nodiscard]]
		const std::vector<base::unique_ptr<ValueParser>>& getPositionalParameters() const;

	private:
		base::unique_ptr<ValueParser>              default_value_parser;
		std::vector<base::unique_ptr<ValueParser>> positional_parameters;
		std::vector<ClapParameter>                 parameters;

		void validateParsing(ParsingResult& result) const;
	};

}  // clap
