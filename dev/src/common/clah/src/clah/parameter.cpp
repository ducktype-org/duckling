/**
 * @file parameter.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "parameter.hpp"

#include <string>

namespace clah {

	const base::Optional<char>& Parameter::getShortName() const { return short_name; }

	const base::Optional<base::RawView>& Parameter::getLongName() const { return long_name; }

	const base::RawView& Parameter::getShortDesc() const { return short_description; }

	const base::Optional<base::RawView>& Parameter::getLongDesc() const { return long_description; }

	MCRef<ValueParser> Parameter::getValueParser() const { return value_parser.ref(); }

	const ParameterNecessity& Parameter::getParameterNecessity() const {
		return parameter_necessity;
	}

	base::Optional<std::string> Parameter::getParameterName() const {
		if_opt_some(long_name, name) return name.stdString();
		// Brace initializer, because name is a char.
		if_opt_some(short_name, name) return { { name } };
		return {};
	}
}
