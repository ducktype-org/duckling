/**
 * @file param_builder.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "param_builder.hpp"
#include "base/variant.hpp"

namespace clap {

	ParamBuilder ParamBuilder::ofValue(base::unique_ptr<ValueParser> value_parser) {
		ParamBuilder builder;
		builder.value_parser     = std::move(value_parser);
		builder.has_value_parser = true;
		return builder;
	}

	ParamBuilder ParamBuilder::ofNonKeyword(base::unique_ptr<ValueParser> value_parser) {
		ParamBuilder& builder = ofValue(std::move(value_parser)).required();
		builder.non_keyword   = true;
		return std::move(builder);
	}

	ParamBuilder ParamBuilder::ofFlag() {
		ParamBuilder& builder = ParamBuilder().optional();
		builder.flag          = true;
		return std::move(builder);
	}

	ParamBuilder& ParamBuilder::addShortName(char new_short_name) {
		short_name = new_short_name;
		return *this;
	}

	ParamBuilder& ParamBuilder::addLongName(base::RawView new_long_name) {
		long_name = new_long_name;
		return *this;
	}

	ParamBuilder& ParamBuilder::addShortDesc(base::RawView new_short_desc) {
		short_description = new_short_desc;
		return *this;
	}

	ParamBuilder& ParamBuilder::addLongDesc(base::RawView new_long_desc) {
		long_description = new_long_desc;
		return *this;
	}

	ParamBuilder& ParamBuilder::optional() {
		parameter_necessity = ParameterNecessity(Optional());
		return *this;
	}

	ParamBuilder& ParamBuilder::required() {
		parameter_necessity = ParameterNecessity(Required());
		return *this;
	}

	ParamBuilder& ParamBuilder::conditional(Conditional::Condition&& condition) {
		parameter_necessity = ParameterNecessity(Conditional(std::move(condition)));
		return *this;
	}

	ConfigParameter ParamBuilder::build() {
		if (!parameter_necessity.has_value())
			throw ParamBuilderException("Every parameter has to have a clear necessity set.");

		// Check all the requirements
		if (!short_name.has_value() && !long_name.has_value())
			throw ParamBuilderException("Every parameter has to have a (short or long) name!");
		if (!short_description.has_value())
			throw ParamBuilderException("Every parameter has to have a short description!");

		if (has_value_parser && value_parser == nullptr)
			throw ParamBuilderException("ValueParser is null!");

		if (flag && has_value_parser) throw ParamBuilderException("Flag should not parse a value!");

		if (non_keyword && !has_value_parser)
			throw ParamBuilderException("Non-keyword parameter should parse a value!");

		variant_match(parameter_necessity.value()) {
			variant_case(Optional, _) {
				if (non_keyword) throw ParamBuilderException("Non-keyword cannot be optional!");
			}
			variant_case(Required, _) {
				if (flag) throw ParamBuilderException("Flag cannot be required!");
			}
			variant_case(Conditional, _) {}
		}

		ConfigParameter parameter;
		parameter.value_parser        = std::move(value_parser);
		parameter.parameter_necessity = parameter_necessity.value();
		parameter.long_name           = std::move(long_name);
		parameter.short_name          = std::move(short_name);
		parameter.short_description   = short_description.value();
		parameter.long_description    = std::move(long_description);
	}

	//	ParamBuilder::ParamBuilder(ParamBuilder&& other) noexcept:
	//		  flag(other.flag),
	//		  non_keyword(other.non_keyword),
	//		  has_value_parser(other.has_value_parser),
	//		  short_name(std::move(other.short_name)),
	//		  long_name(std::move(other.long_name)),
	//		  short_description(std::move(other.short_description)),
	//		  long_description(std::move(other.long_description)),
	//		  value_parser(std::move(other.value_parser)),
	//		  parameter_necessity(std::move(other.parameter_necessity)) {}
}
