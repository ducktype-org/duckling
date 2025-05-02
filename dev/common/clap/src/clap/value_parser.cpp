/**
 * @file value_parser.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */


#include "value_parser.hpp"

#include "exceptions.hpp"

#include <filesystem/file.hpp>

#include <charconv>

namespace clap {

	FileParser::FileParser(): ValueParser(), file_regex{".*"} {}

	FileParser::FileParser(regex::Regex regex): file_regex(std::move(regex)) {}

	FileParser::FileParser(const std::string& name, regex::Regex regex):
			ValueParser(name),
			file_regex(std::move(regex)) {}

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

		return { data, data, position };
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
		return { value, std::string(raw_input.substr(start, end_index - start)), end_index };
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

			return { Range{ value_left, value_right }, std::string(my_chunk), position };
		} catch (clap::exceptions::ValueParsingException& e) {
			throw exceptions::ValueParsingException(
				getTypeName().c_str(), start, position, raw_input, "Error parsing range's values"
			);
		}
	}

	ValueParsingResult FileParser::parse(usize start, std::string_view raw_input) const {
		auto result = StringParser::make()->parse(start, raw_input);
		auto str    = std::any_cast<std::string>(result.value);

		if (!file_regex.match(str))
			throw clap::exceptions::ValueParsingException(
				getTypeName().c_str(),
				start,
				result.position,
				raw_input,
				"argument does not match regex"  // unluckily, there is no way to extract the regex
			                                     // from file_regex.
			);

		std::filesystem::path path = str;

		if (!std::filesystem::exists(path)) throw clap::exceptions::FileDoesNotExist(path);

		fs::FilePath file(path);

		return { file, result.raw_source, result.position };
	}
}
