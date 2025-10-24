/**
 * @file param_builder.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "param_builder.hpp"

#include "exceptions.hpp"

namespace clah {

	ParamBuilder ParamBuilder::ofValue(Box<ValueParser> value_parser) {
		ParamBuilder builder;
		builder.value_parser = std::move(value_parser);
		return builder;
	}

	ParamBuilder ParamBuilder::ofFlag() { return std::move(ParamBuilder().optional()); }

	ParamBuilder& ParamBuilder::addShortName(char new_short_name) {
		short_name = new_short_name;
		return *this;
	}

	ParamBuilder& ParamBuilder::addLongName(base::RawView new_long_name) {
		if (new_long_name.size() <= 1)
			throw exceptions::ClahException(base::strConcat(
				"Name should be longer than 1 character, but received \"", new_long_name, "\"."
			));

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
		if (!value_parser) throw ParamBuilderException("Flag cannot be required!");
		return *this;
	}

	ParamBuilder& ParamBuilder::conditional(
		Conditional::Condition&& condition, const std::string& description
	) {
		parameter_necessity = ParameterNecessity(Conditional{
			.condition             = std::move(condition),
			.condition_description = description,
		});
		return *this;
	}

	Parameter ParamBuilder::build() {
		// Check all the requirements
		if (!short_name.has_value() && !long_name.has_value())
			throw ParamBuilderException("Every parameter has to have a (short or long) name!");
		if (!short_description.has_value())
			throw ParamBuilderException("Every parameter has to have a short description!");

		Parameter parameter;
		parameter.value_parser        = std::move(value_parser);
		parameter.parameter_necessity = parameter_necessity.value();
		parameter.long_name           = long_name;
		parameter.short_name          = short_name;
		parameter.short_description   = short_description.value();
		parameter.long_description    = long_description;
		return parameter;
	}
}
