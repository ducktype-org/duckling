/**
 * @file value_parser.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */


#include "value_parser.hpp"

#include "exceptions.hpp"

#include <filesystem/file.hpp>

#include <charconv>

namespace clah {
	ValueParsingResult StringParser::parse(usize start, std::string_view raw_input) const {
		// Allows parsing of strings like "\"Hello\\\" here\" and the\"re!".
		usize position        = start;
		usize quotes_to_close = raw_input[position] == '\"';

		if (quotes_to_close > 0) position++;

		std::string data;
		while (position < raw_input.size()
		       && (quotes_to_close || !std::isspace(raw_input[position]))) {
			if (raw_input[position] == '\"') {
				if (!data.empty() && data.back() == '\\')
					data.pop_back();
				else {
					quotes_to_close--;
					position++;
					continue;
				}
			}

			data.push_back(raw_input[position++]);
		}

		return { .value = data, .raw_source = data, .position = position };
	}

	ValueParsingResult IntParser::parse(usize start, std::string_view raw_input) const {
		usize end_index = start;
		while (end_index < raw_input.size() && !std::isspace(raw_input[end_index])) end_index++;

		i64  value  = 0;
		auto begin  = raw_input.begin() + start;
		auto end    = raw_input.begin() + end_index;
		auto result = std::from_chars(begin, end, value, 10);
		if (result.ptr != end or result.ec == std::errc::invalid_argument
		    or result.ec == std::errc::result_out_of_range) {
			throw exceptions::ValueParsingException(
				getTypeName().c_str(), start, end_index, raw_input
			);
		}
		return { .value      = value,
			     .raw_source = std::string(raw_input.substr(start, end_index - start)),
			     .position   = end_index };
	}

	ValueParsingResult RangeParser::parse(usize start, std::string_view raw_input) const {
		usize position = start;
		while (position < raw_input.size() && !std::isspace(raw_input[position])) position++;

		std::string_view my_chunk    = raw_input.substr(start, position - start + 1);
		auto             dot_dot_pos = my_chunk.find("..");
		if (my_chunk.find("..") == std::string_view::npos)
			throw exceptions::ValueParsingException(
				getTypeName().c_str(),
				start,
				position,
				raw_input,
				"There should be \"..\" between values, like 1..4 == 1, 2, 3"
			);

		auto left_value_source = my_chunk.substr(0, dot_dot_pos);
		auto right_value_source
			= my_chunk.substr(dot_dot_pos + 2, my_chunk.size() - dot_dot_pos - 2);
		try {
			auto parser      = IntParser::make();
			i64  value_left  = std::any_cast<i64>(parser->parse(0, left_value_source).value);
			i64  value_right = std::any_cast<i64>(parser->parse(0, right_value_source).value);

			return { .value      = Range{ .begin = value_left, .end = value_right },
				     .raw_source = std::string(my_chunk),
				     .position   = position };
		} catch (clah::exceptions::ValueParsingException& e) {
			throw exceptions::ValueParsingException(
				getTypeName().c_str(), start, position, raw_input, "Error parsing range's values"
			);
		}
	}

	ValueParsingResult FileParser::parse(usize start, std::string_view raw_input) const {
		auto result = StringParser::make()->parse(start, raw_input);
		auto str    = std::any_cast<std::string>(result.value);

		std::smatch match;
		if (!std::regex_match(str, match, file_regex))
			throw clah::exceptions::ValueParsingException(
				getTypeName().c_str(),
				start,
				result.position,
				raw_input,
				"argument does not match regex"  // unluckily, there is no way to extract the regex
			                                     // from file_regex.
			);

		std::filesystem::path path = str;

		if (!std::filesystem::exists(path)) throw clah::exceptions::FileDoesNotExist(path);

		fs::File file(path);

		return { .value = file, .raw_source = result.raw_source, .position = result.position };
	}

	ValueParsingResult FilePathParser::parse(usize start, std::string_view raw_input) const {
		auto result = StringParser::make()->parse(start, raw_input);
		auto str    = std::any_cast<std::string>(result.value);

		std::smatch r_match;
		if (!std::regex_match(str, r_match, filepath_regex))
			throw clah::exceptions::ValueParsingException(
				getTypeName().c_str(),
				start,
				result.position,
				raw_input,
				"argument does not match regex"
			);

		std::filesystem::path path = str;

		fs::FilePath filepath(path);

		return { .value = filepath, .raw_source = result.raw_source, .position = result.position };
	}

	ValueParsingResult StringListParser::parse(usize start, std::string_view raw_input) const {
		auto string_result
			= base::anyCast<std::string>(StringParser::make()->parse(start, raw_input).value);

		// trim spaces from both ends:
		auto trim_spaces = [](std::string_view sv) -> std::string {
			std::string string_result_trimmed{ sv };
			string_result_trimmed.erase(0, string_result_trimmed.find_first_not_of(" \t\n\r\f\v"));
			string_result_trimmed.erase(string_result_trimmed.find_last_not_of(" \t\n\r\f\v") + 1);
			return string_result_trimmed;
		};

		auto string_result_trimmed = trim_spaces(string_result);

		if (string_result_trimmed.size() < 2 or string_result_trimmed.front() != '['
		    or string_result_trimmed.back() != ']') {
			throw exceptions::ValueParsingException(
				getTypeName().c_str(),
				start,
				start + string_result.size(),
				raw_input,
				"String list must be encapsulated in [ ]."
			);
		}

		auto string_list_content
			= string_result_trimmed.substr(1, string_result_trimmed.size() - 2);

		// split by commas:
		std::vector<std::string> values;
		usize                    pos = 0;

		while (pos < string_list_content.size()) {
			auto comma_pos = string_list_content.find(',', pos);
			if (comma_pos == std::string::npos) comma_pos = string_list_content.size();

			auto value = trim_spaces(string_list_content.substr(pos, comma_pos - pos));
			if (!value.empty()) values.emplace_back(value);

			pos = comma_pos + 1;
		}

		return {
			.value      = values,
			.raw_source = string_result,
			.position   = start + string_result.size(),
		};
	}
}
