/**
 * @file value_parser.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */


#include "value_parser.hpp"

namespace clap {
	ValueParsingResult StringParser::parse(usize start, const std::string& raw_input) const {
		usize       position           = start;
		bool        started_with_quote = raw_input[position] == '\"';
		std::string data;
		if (started_with_quote) {
			while (position < raw_input.size()) {
				if (raw_input[position] == '\"') {
					if (!data.empty() && data.back() == '\\')
						data += raw_input[position];
					else
						break;
				}
			}
		} else {
			while (!std::isspace(raw_input[position])) data += raw_input[position++];
		}
		return { data, data, position };
	}
}
