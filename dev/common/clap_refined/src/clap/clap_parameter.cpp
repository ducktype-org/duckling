/**
 * @file clap_parameter.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "clap_parameter.hpp"

namespace clap {

	const base::Optional<char>& ClapParameter::getShortName() const { return short_name; }

	const base::Optional<base::RawView>& ClapParameter::getLongName() const { return long_name; }

	const base::RawView& ClapParameter::getShortDesc() const { return short_description; }

	const base::Optional<base::RawView>& ClapParameter::getLongDesc() const {
		return long_description;
	}

	const ValueParser* ClapParameter::getValueParser() const { return value_parser.get(); }

	const ParameterNecessity& ClapParameter::getParameterNecessity() const {
		return parameter_necessity;
	}
}
