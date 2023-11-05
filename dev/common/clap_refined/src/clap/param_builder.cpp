/**
 * @file param_builder.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "param_builder.hpp"
#include "base/variant.hpp"

namespace clap {

	ParamBuilder ParamBuilder::ofValue(base::unique_ptr<ValueParser> value_parser) {
		ParamBuilder builder;
		if (value_parser == nullptr) throw ParamBuilderException("ValueParser is null!");
		builder.value_parser = std::move(value_parser);
		return builder;
	}

	ParamBuilder ParamBuilder::ofFlag() { return std::move(ParamBuilder().optional()); }

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

	ParamBuilder& ParamBuilder::conditional(
		Conditional::Condition&& condition, const std::string& description
	) {
		parameter_necessity = ParameterNecessity(Conditional(std::move(condition), description));
		return *this;
	}

	ClapParameter ParamBuilder::build() {
		// Check all the requirements
		if (!short_name.has_value() && !long_name.has_value())
			throw ParamBuilderException("Every parameter has to have a (short or long) name!");
		if (!short_description.has_value())
			throw ParamBuilderException("Every parameter has to have a short description!");

		variant_match(parameter_necessity.value()) {
			variant_case(Required, _) {
				if (value_parser == nullptr)
					throw ParamBuilderException("Flag cannot be required!");
			}
			variant_default {}
		}

		ClapParameter parameter;
		parameter.value_parser        = std::move(value_parser);
		parameter.parameter_necessity = parameter_necessity.value();
		parameter.long_name           = std::move(long_name);
		parameter.short_name          = std::move(short_name);
		parameter.short_description   = short_description.value();
		parameter.long_description    = std::move(long_description);
		return parameter;
	}
}
