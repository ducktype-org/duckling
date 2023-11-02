/**
 * @file value_parser.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */


#include "value_parser.hpp"

namespace clap {
	bool ValueParser::doesAcceptMultipleValues() const { return accept_multiple_values; }

	const base::RawView& ValueParser::getDelimiter() const { return delimiter; }
}
