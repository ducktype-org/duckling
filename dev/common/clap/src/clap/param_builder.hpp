/**
 * @file param_builder.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include <utility>

#include "value_parser.hpp"
#include "parameter.hpp"

namespace clap {

	// This exception is not meant to be handled, as it means clap::Parameter
	// has been built inappropriately.
	struct ParamBuilderException: std::exception {
		std::string what_str;

	public:
		explicit ParamBuilderException(std::string what_str): what_str(std::move(what_str)) {}

		const char* what() const noexcept override { return what_str.c_str(); }
	};

	class ParamBuilder {
	public:
		static ParamBuilder ofValue(base::unique_ptr<ValueParser> value_parser);
		static ParamBuilder ofFlag();

		ParamBuilder& addShortName(char new_short_name);
		ParamBuilder& addLongName(base::RawView new_long_name);
		ParamBuilder& addShortDesc(base::RawView new_short_desc);
		ParamBuilder& addLongDesc(base::RawView new_long_desc);

		ParamBuilder& optional();
		ParamBuilder& required();
		ParamBuilder&
			conditional(Conditional::Condition&& condition, const std::string& description = "");

		Parameter build();

	private:
		ParamBuilder() { optional(); }

		base::Optional<char>          short_name;
		base::Optional<base::RawView> long_name;
		base::Optional<base::RawView> short_description;
		base::Optional<base::RawView> long_description;

		base::unique_ptr<ValueParser> value_parser;

		base::Optional<ParameterNecessity> parameter_necessity;
	};
}
