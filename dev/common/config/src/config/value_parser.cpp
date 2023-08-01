#include "value_parser.hpp"
#include <charconv>

namespace config {
	std::any StringParser::parse(base::RawView data) {
		// @TODO: error handling here
		return data.stdString();
	}
	std::string StringParser::helperMess() {
		return "<string>";
	}

	std::any IntParser::parse(base::RawView data) {
		i64 value = 0;
		auto begin = reinterpret_cast<const char*>(data.getBegin());
		auto end = reinterpret_cast<const char*>(data.getBegin() + data.size());
		auto result = std::from_chars(begin, end, value, 10);

		if (result.ptr != end
				or result.ec == std::errc::invalid_argument
				or result.ec == std::errc::result_out_of_range) {
			throw WrongParamValue("IntType");
		}
		return value;
	}
	std::string IntParser::helperMess() {
		return "<int>";
	}
}
