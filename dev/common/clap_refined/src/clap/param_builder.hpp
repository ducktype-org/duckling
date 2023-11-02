/**
 * @file param_builder.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include "value_parser.hpp"
#include "config_parameter.hpp"

namespace clap {

	class ParamBuilderException: public base::LogicError {
		using base::LogicError::LogicError;
	};

	class ParamBuilder {
	public:
		static ParamBuilder ofValue(base::unique_ptr<ValueParser> value_parser);
		static ParamBuilder ofNonKeyword(base::unique_ptr<ValueParser> value_parser);
		static ParamBuilder ofFlag();

		ParamBuilder& addShortName(base::RawView new_short_name);
		ParamBuilder& addLongName(base::RawView new_long_name);
		ParamBuilder& addShortDesc(base::RawView new_short_desc);
		ParamBuilder& addLongDesc(base::RawView new_long_desc);

		ParamBuilder& optional();
		ParamBuilder& required();
		ParamBuilder& conditional(Conditional::Condition&& condition);

		ConfigParameter build();

	private:
		ParamBuilder() = default;
		//		ParamBuilder(ParamBuilder&& other) noexcept;

		bool flag             = false;
		bool non_keyword      = false;
		bool has_value_parser = false;

		base::Optional<base::RawView> short_name;
		base::Optional<base::RawView> long_name;
		base::Optional<base::RawView> short_description;
		base::Optional<base::RawView> long_description;

		base::unique_ptr<ValueParser> value_parser;

		base::Optional<ParameterNecessity> parameter_necessity;
	};
}
