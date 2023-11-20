/**
 * @file param_builder.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include "value_parser.hpp"
#include "clap_parameter.hpp"

namespace clap {

	class ParamBuilderException: public base::LogicError {
		using base::LogicError::LogicError;
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

		ClapParameter build();

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
