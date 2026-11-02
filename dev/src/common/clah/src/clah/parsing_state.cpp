// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "parsing_state.hpp"

#include "exceptions.hpp"

/**
 * Basic helper functions.
 */
namespace {
	/**
	 * Parses command line arguments from argc and argv. Assumes argv[0] is the program name.
	 * The command line arguments are assumed to be already parsed by the shell,
	 * so there are no quotes and spaces at the beginning or end of arguments.
	 */
	std::vector<std::string> fromArgcv(usize argc, const char* const* argv) {
		// Merge args with spaces between.
		std::vector<std::string> args;

		// if an argv[i] contains a white space, then it must have been added with quotes
		for (usize i = 1; i < argc; i++) {
			CORE_ASSERT(argv[i] != nullptr, "Clah received null pointer as one of argv arguments.");

			auto arg = std::string(argv[i]);

			args.push_back(arg);
		}
		return args;
	}

	/**
	 * Performs the shell-like parsing of a command line string. It splits the string by spaces, but
	 * respects quotes, which is the same parsing as a typical shell would do. The quotes are not
	 * included in the resulting arguments.
	 */
	std::vector<std::string> fromDirectString(const std::string& args) {
		std::vector<std::string> result;
		std::string              current_arg;
		bool                     in_quotes = false;
		for (char c: args) {
			if (c == '"') {
				in_quotes = !in_quotes;
			} else if (std::isspace(c) && not in_quotes) {
				if (!current_arg.empty()) {
					result.push_back(current_arg);
					current_arg.clear();
				}
			} else {
				current_arg += c;
			}
		}
		if (!current_arg.empty()) result.push_back(current_arg);
		return result;
	}

	std::string withQuotes(std::string str) {
		if (str.find(' ') != std::string::npos)
			return "\"" + str + "\"";
		else
			return str;
	}

	/**
	 * @brief Get the merged view of the arguments. Used for error messages.
	 */
	std::string mergedArguments(const std::vector<std::string>& words) {
		std::string args;
		for (usize i = 0; i < words.size(); ++i) {
			args += withQuotes(words[i]);
			if (i + 1 < words.size()) args += ' ';
		}
		return args;
	}

	usize calculatePositionInMerged(
		const std::vector<std::string>& words, usize word_index, usize inside_word_position
	) {
		if (word_index >= words.size()) return mergedArguments(words).size() - 1;

		usize position = 0;
		for (usize i = 0; i < word_index; ++i)
			position += withQuotes(words[i]).size() + 1;  // +1 for the space between arguments
		// If the last word has quotes, we add 1 for the opening quote
		position += withQuotes(words[word_index]).size() - (words[word_index].size()) / 2;
		position += inside_word_position;

		return position;
	}
}

namespace clah {
	ParsingState::ParsingState(const std::string& args):
		  words(fromDirectString(args)),
		  merged_view(mergedArguments(words)),
		  result("", args) {}

	ParsingState::ParsingState(usize argc, const char* const* argv) {
		words       = fromArgcv(argc, argv);
		merged_view = mergedArguments(words);

		// Check if program is invoked using "./" or by name.
		std::string program_name = argv[0];
		if (program_name.starts_with("./")) program_name = program_name.substr(2);

		result = clah::ParsingResult(program_name, merged_view);
	}

	void ParsingState::parsePositional(const clah::ValueParser& parser) {
		clah::ParsedValue value = parseValueWithParser(parser);
		result.insertPositional(value);
	}

	void ParsingState::parseExtra(const clah::ValueParser& parser) {
		clah::ParsedValue value = parseValueWithParser(parser);
		result.insertExtra(value);
	}

	void ParsingState::parseParameter(const std::vector<clah::Parameter>& params) {
		auto [param_name, name_type] = parseName();
		if (name_type == NameType::EmptyName)
			throw clah::exceptions::ExpectedParameterIdentifier(position_in_merged, merged_view);

		if (name_type == NameType::LongName) {
			if (!findParameterAndParse(params, param_name, name_type))
				throw clah::exceptions::InvalidParameterName(param_name);
		} else {
			for (char c: param_name)
				if (!findParameterAndParse(params, { c }, name_type))
					throw clah::exceptions::InvalidParameterName(param_name);
		}
	}

	bool ParsingState::isEnd() const {
		bool is_inside
			= current_word < words.size() && inside_word_position < words[current_word].size();
		return not is_inside;
	}

	std::pair<std::string, NameType> ParsingState::parseName() {
		CORE_ASSERT(frontChar() == '-', "parseName expected '-'");
		usize hyphen_count = advanceUntilInWord([](char c) { return c != '-'; });
		if (isEnd()) {
			// We reached the end of arguments while parsing the name, which means that there is no
			// name after dashes, which is invalid.
			throw clah::exceptions::ExpectedParameterIdentifier(position_in_merged, merged_view);
		}

		std::string name       = frontWord();
		usize       name_count = advanceUntilInWord([](char c) { return c == '='; });
		name                   = name.substr(0, name_count);

		if (not isEnd() && frontChar() == '=') advanceChar();

		if (name.empty()) return { "", NameType::EmptyName };
		return { name, hyphen_count == 1 ? NameType::ShortName : NameType::LongName };
	}

	bool ParsingState::findParameterAndParse(
		const std::vector<clah::Parameter>& parameters,
		const std::string&                  param_name,
		NameType                            name_type
	) {
		for (auto& parameter: parameters) {
			bool found_parameter = false;
			if (name_type == NameType::LongName) {
				if_opt_some(parameter.getLongName(), name) {
					if (name == param_name.c_str()) found_parameter = true;
				}
			} else {
				if_opt_some(parameter.getShortName(), name) {
					if (name == param_name[0]) found_parameter = true;
				}
			}
			if (found_parameter) {
				parseWithParameter(parameter, param_name);
				return true;
			}
		}
		return false;
	}

	void ParsingState::parseWithParameter(const clah::Parameter& parameter, const std::string& name) {
		if (parameter.getValueParser() == nullptr) {
			// then it's a flag
			result.insertFlag(parameter);
		} else {
			// Throw if duplicated
			if (result.hasParam(parameter)) throw clah::exceptions::DuplicatedParameter(name);
			if (isEnd())
				throw clah::exceptions::ParameterRequiresValue(
					name, parameter.getValueParser()->getTypeName()
				);

			clah::ParsedValue parsed = parseValueWithParser(*parameter.getValueParser());
			result.insertParameterValue(parameter, parsed);
		}
	}

	clah::ParsedValue ParsingState::parseValueWithParser(const clah::ValueParser& parser) {
		std::string argument = frontWord();
		advanceWord();

		clah::ValueParsingResult parsed = parser.parse(argument);
		return clah::ParsedValue{ .value = parsed.value, .raw_source = parsed.raw_source };
	}

	void ParsingState::advanceChar() {
		inside_word_position++;
		if (inside_word_position >= words[current_word].size()) {
			current_word++;
			inside_word_position = 0;
		}

		position_in_merged = calculatePositionInMerged(words, current_word, inside_word_position);
	}

	void ParsingState::advanceWord() {
		current_word++;
		inside_word_position = 0;

		position_in_merged = calculatePositionInMerged(words, current_word, inside_word_position);
	}

	char ParsingState::frontChar() const {
		if (isEnd()) {
			throw exceptions::ClahException(
				"Parser error: frontChar() called with no more arguments"
			);
		}
		return words[current_word][inside_word_position];
	}

	std::string ParsingState::frontWord() const {
		if (isEnd()) {
			throw exceptions::ClahException(
				"Parser error: frontWord() called with no more arguments"
			);
		}
		return words[current_word].substr(inside_word_position);
	}

	usize ParsingState::advanceUntilInWord(const std::function<bool(char)>& condition) {
		usize count      = 0;
		usize start_word = current_word;
		while (current_word == start_word && !condition(frontChar())) {
			advanceChar();
			count++;
		}
		return count;
	}
}
