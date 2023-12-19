/**
 * @file clap_parameter.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "parameter.hpp"

namespace clap {

	const base::Optional<char>& Parameter::getShortName() const { return short_name; }

	const base::Optional<base::RawView>& Parameter::getLongName() const { return long_name; }

	const base::RawView& Parameter::getShortDesc() const { return short_description; }

	const base::Optional<base::RawView>& Parameter::getLongDesc() const { return long_description; }

	const ValueParser* Parameter::getValueParser() const { return value_parser.get(); }

	const ParameterNecessity& Parameter::getParameterNecessity() const {
		return parameter_necessity;
	}
}
