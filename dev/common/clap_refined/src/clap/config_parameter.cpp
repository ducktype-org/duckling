/**
 * @file config_parameter.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "config_parameter.hpp"

namespace clap {

	const base::Optional<char>& ConfigParameter::getShortName() const { return short_name; }

	const base::Optional<base::RawView>& ConfigParameter::getLongName() const { return long_name; }

	const base::RawView& ConfigParameter::getShortDesc() const { return short_description; }

	const base::Optional<base::RawView>& ConfigParameter::getLongDesc() const {
		return long_description;
	}

	const ValueParser* ConfigParameter::getValueParser() const { return value_parser.get(); }

	const ParameterNecessity& ConfigParameter::getParameterNecessity() const {
		return parameter_necessity;
	}
}
