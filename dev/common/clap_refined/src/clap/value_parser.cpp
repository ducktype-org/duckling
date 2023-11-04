/**
 * @file value_parser.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */


#include <charconv>
#include "value_parser.hpp"

namespace clap {
	ValueParsingResult StringParser::parse(usize start, const std::string_view& raw_input) const {
		usize       position           = start;
		bool        started_with_quote = raw_input[position] == '\"';
		std::string data;
		if (started_with_quote) {
			position++;
			while (position < raw_input.size()) {
				if (raw_input[position] == '\"') {
					if (data.empty() || data.back() == '\\') {
						data.pop_back();
						data += raw_input[position++];
					} else {
						position++;
						break;
					}
				} else {
					data += raw_input[position++];
				}
			}
		} else {
			while (position < raw_input.size() && !std::isspace(raw_input[position]))
				data += raw_input[position++];
		}
		return { data, data, position };
	}

	ValueParsingResult IntParser::parse(usize start, const std::string_view& raw_input) const {
		usize end_index = start;
		while (end_index < raw_input.size() && !std::isspace(raw_input[end_index])) end_index++;

		i64  value  = 0;
		auto begin  = raw_input.begin() + start;
		auto end    = raw_input.begin() + end_index;
		auto result = std::from_chars(begin, end, value, 10);
		if (result.ptr != end or result.ec == std::errc::invalid_argument
		    or result.ec == std::errc::result_out_of_range) {
			//			throw WrongParamValue("IntType");
			throw base::LogicError("Wrong param value!");
		}
		return { value, std::string(raw_input.substr(start, end_index - start)), end_index };
	}
}
