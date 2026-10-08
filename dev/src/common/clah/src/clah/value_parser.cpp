// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file value_parser.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */


#include "value_parser.hpp"

#include "exceptions.hpp"

#include <base/str/str_utils.hpp>

#include <filesystem/file.hpp>

#include <algorithm>
#include <charconv>

namespace clah {

	namespace utils {
		std::vector<std::string> splitCommaSeparated(std::string_view value) {
			std::vector<std::string> values;
			usize                    pos = 0;

			while (pos < value.size()) {
				auto comma_pos = value.find(',', pos);
				if (comma_pos == std::string_view::npos) comma_pos = value.size();

				auto item = base::strTrim(value.substr(pos, comma_pos - pos));
				if (!item.empty()) values.emplace_back(item);

				pos = comma_pos + 1;
			}

			return values;
		}
	}

	namespace {
		bool containsCategory(
			const std::vector<std::string>& categories, std::string_view candidate
		) {
			return std::ranges::find(categories, candidate) != categories.end();
		}
	}

	ValueParsingResult StringParser::parse(std::string_view argument) const {
		return { .value = std::string(argument), .raw_source = std::string(argument) };
	}

	ValueParsingResult IntParser::parse(std::string_view argument) const {
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

	ValueParsingResult RangeParser::parse(std::string_view argument) const {
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

	ValueParsingResult FileParser::parse(std::string_view argument) const {
		std::cmatch match;
		if (!std::regex_match(argument.begin(), argument.end(), match, file_regex))
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
		bool acceptable_type = std::filesystem::is_regular_file(path)
		                    or (accept_directories and std::filesystem::is_directory(path));
		if (!acceptable_type) throw clah::exceptions::NotARegularFile(path, accept_directories);

		fs::File file(path);

		return { .value = file, .raw_source = std::string(argument) };
	}

	ValueParsingResult FilePathParser::parse(std::string_view argument) const {
		std::cmatch r_match;
		if (!std::regex_match(argument.begin(), argument.end(), r_match, filepath_regex))
			throw clah::exceptions::ValueParsingException(
				getTypeName().c_str(),
				0,
				argument.empty() ? 0 : argument.size() - 1,
				argument,
				"argument does not match regex"
			);

		std::filesystem::path path = argument;

		fs::FilePath filepath(path);

		return { .value = filepath, .raw_source = std::string(argument) };
	}

	std::string CategoryParser::debugPrintCategories(const std::vector<std::string>& categories) {
		std::string result;
		for (usize i = 0; i < categories.size(); ++i) {
			result += categories[i];
			if (i + 1 < categories.size()) result += ", ";
		}
		return result;
	}

	ValueParsingResult CategoryParser::parse(std::string_view argument) const {
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

		return { .value = std::string(argument), .raw_source = std::string(argument) };
	}
}
