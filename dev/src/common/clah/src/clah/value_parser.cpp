/**
 * @file value_parser.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */


#include "value_parser.hpp"

#include "exceptions.hpp"

#include <filesystem/file.hpp>

#include <charconv>

namespace clah {
	namespace {
		std::string trimSpaces(std::string_view sv) {
			auto first = sv.find_first_not_of(" \t\n\r\f\v");
			if (first == std::string_view::npos) return {};

			auto last = sv.find_last_not_of(" \t\n\r\f\v");
			return std::string(sv.substr(first, last - first + 1));
		}

		std::vector<std::string> splitCommaSeparated(const std::string& value) {
			std::vector<std::string> values;
			usize                    pos = 0;

			while (pos < value.size()) {
				auto comma_pos = value.find(',', pos);
				if (comma_pos == std::string::npos) comma_pos = value.size();

				auto item = trimSpaces(std::string_view(value).substr(pos, comma_pos - pos));
				if (!item.empty()) values.emplace_back(item);

				pos = comma_pos + 1;
			}

			return values;
		}

		bool containsCategory(
			const std::vector<std::string>& categories, const std::string& candidate
		) {
			return std::find(categories.begin(), categories.end(), candidate) != categories.end();
		}
	}

	ValueParsingResult StringParser::parse(const std::string& argument) const {
		return { .value = argument, .raw_source = argument };
	}

	ValueParsingResult IntParser::parse(const std::string& argument) const {
		auto begin = argument.data();
		auto end   = begin + argument.size();

		i64  value  = 0;
		auto result = std::from_chars(begin, end, value, 10);
		if (result.ptr != end or result.ec == std::errc::invalid_argument
		    or result.ec == std::errc::result_out_of_range) {
			usize end_index = argument.empty() ? 0 : argument.size() - 1;
			throw exceptions::ValueParsingException(getTypeName().c_str(), 0, end_index, argument);
		}
		return { .value = value, .raw_source = std::string(argument) };
	}

	ValueParsingResult RangeParser::parse(const std::string& argument) const {
		auto dot_dot_pos = argument.find("..");
		if (dot_dot_pos == std::string::npos)
			throw exceptions::ValueParsingException(
				getTypeName().c_str(),
				0,
				argument.empty() ? 0 : argument.size() - 1,
				argument,
				"There should be \"..\" between values, like 1..4 == 1, 2, 3"
			);

		auto left_value_source = argument.substr(0, dot_dot_pos);
		auto right_value_source
			= argument.substr(dot_dot_pos + 2, argument.size() - dot_dot_pos - 2);
		try {
			auto parser = IntParser::make();
			i64  value_left
				= std::any_cast<i64>(parser->parse({ std::string(left_value_source) }).value);
			i64 value_right
				= std::any_cast<i64>(parser->parse({ std::string(right_value_source) }).value);

			return { .value      = Range{ .begin = value_left, .end = value_right },
				     .raw_source = std::string(argument) };
		} catch (clah::exceptions::ValueParsingException& e) {
			throw exceptions::ValueParsingException(
				getTypeName().c_str(),
				0,
				argument.empty() ? 0 : argument.size() - 1,
				argument,
				"Error parsing range's values"
			);
		}
	}

	ValueParsingResult FileParser::parse(const std::string& argument) const {
		std::smatch match;
		if (!std::regex_match(argument, match, file_regex))
			throw clah::exceptions::ValueParsingException(
				getTypeName().c_str(),
				0,
				argument.empty() ? 0 : argument.size() - 1,
				argument,
				"argument does not match regex"  // unluckily, there is no way to extract the regex
			                                     // from file_regex.
			);

		std::filesystem::path path = argument;

		if (!std::filesystem::exists(path)) throw clah::exceptions::FileDoesNotExist(path);

		fs::File file(path);

		return { .value = file, .raw_source = argument };
	}

	ValueParsingResult FilePathParser::parse(const std::string& argument) const {
		std::smatch r_match;
		if (!std::regex_match(argument, r_match, filepath_regex))
			throw clah::exceptions::ValueParsingException(
				getTypeName().c_str(),
				0,
				argument.empty() ? 0 : argument.size() - 1,
				argument,
				"argument does not match regex"
			);

		std::filesystem::path path = argument;

		fs::FilePath filepath(path);

		return { .value = filepath, .raw_source = argument };
	}

	ValueParsingResult StringListParser::parse(const std::string& argument) const {
		std::vector<std::string> values = splitCommaSeparated(argument);

		return {
			.value      = values,
			.raw_source = argument,
		};
	}

	std::string CategoryParser::debugPrintCategories(const std::vector<std::string>& categories) {
		std::string result;
		for (usize i = 0; i < categories.size(); ++i) {
			result += categories[i];
			if (i + 1 < categories.size()) result += ", ";
		}
		return result;
	}

	ValueParsingResult CategoryParser::parse(const std::string& argument) const {
		if (categories.empty()) {
			throw exceptions::ValueParsingException(
				getTypeName().c_str(),
				0,
				argument.empty() ? 0 : argument.size() - 1,
				argument,
				"No categories configured for parser"
			);
		}

		if (!containsCategory(categories, argument)) {
			throw exceptions::ValueParsingException(
				getTypeName().c_str(),
				0,
				argument.empty() ? 0 : argument.size() - 1,
				argument,
				"Invalid category. Allowed: " + CategoryParser::debugPrintCategories(categories)
			);
		}

		return { .value = argument, .raw_source = argument };
	}

	ValueParsingResult CategoryListParser::parse(const std::string& argument) const {
		if (categories.empty()) {
			throw exceptions::ValueParsingException(
				getTypeName().c_str(),
				0,
				argument.empty() ? 0 : argument.size() - 1,
				argument,
				"No categories configured for parser"
			);
		}

		std::vector<std::string> values = splitCommaSeparated(argument);
		for (const auto& value: values) {
			if (!containsCategory(categories, value)) {
				throw exceptions::ValueParsingException(
					getTypeName().c_str(),
					0,
					argument.empty() ? 0 : argument.size() - 1,
					argument,
					"Invalid category in list: \"" + value
						+ "\". Allowed: " + CategoryParser::debugPrintCategories(categories)
				);
			}
		}

		return { .value = values, .raw_source = argument };
	}
}
