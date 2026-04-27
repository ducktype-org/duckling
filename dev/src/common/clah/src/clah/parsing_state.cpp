#include "parsing_state.hpp"

#include "exceptions.hpp"

#include <iostream>

/**
 * Basic helper functions.
 */
namespace {
	/**
	 * Merges the arguments provided in a form of C-string array with spaces. If a C-string
	 * contains a white space, then adds quotes around it.
	 * @note: It skips first parameter, as it is assumed to be the name of the command.
	 * @param argc Argument count.
	 * @param argv Argument vector - the array of C-strings.
	 * @return Merged vector into a single string.
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

	std::string mergeArgs(const std::vector<std::string>& args) {
		std::string merged_args;
		for (const auto& arg: args) merged_args += arg + ' ';
		if (!merged_args.empty()) merged_args.pop_back();
		return merged_args;
	}
}

namespace clah {
	ParsingState::ParsingState(const std::string& args):
		  words(fromDirectString(args)),
		  result("", args) {}

	ParsingState::ParsingState(usize argc, const char* const* argv) {
		words = fromArgcv(argc, argv);

		// Check if program is invoked using "./" or by name.
		std::string program_name = argv[0];
		if (program_name.starts_with("./")) program_name = program_name.substr(2);

		result = clah::ParsingResult(program_name, mergeArgs(words));
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
			throw clah::exceptions::ExpectedParameterIdentifier(
				(i32) currentPosition(), mergedArguments()
			);

		if (name_type == NameType::LongName) {
			if (!findParameterAndParse(params, param_name, name_type))
				throw clah::exceptions::InvalidParameterName(param_name);
		} else {
			for (char c: param_name)
				if (!findParameterAndParse(params, { c }, name_type))
					throw clah::exceptions::InvalidParameterName(param_name);
		}
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

	bool ParsingState::isEnd() const {
		bool is_inside
			= current_word < words.size() && inside_word_position < words[current_word].size();
		return not is_inside;
	}

	std::pair<std::string, NameType> ParsingState::parseName() {
		CORE_ASSERT(frontChar() == '-', "parseName expected '-'");
		size_t hyphen_count = advanceUntilInWord([](char c) { return c != '-'; });
		if (isEnd()) {
			// We reached the end of arguments while parsing the name, which means that there is no
			// name after dashes, which is invalid.
			throw clah::exceptions::ExpectedParameterIdentifier(
				(i32) currentPosition(), mergedArguments()
			);
		}

		std::string name = frontWord();
		size_t name_count = advanceUntilInWord([](char c) { return c == '='; });
		name = name.substr(0, name_count);

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

	std::string withQuotes(std::string str) {
		if (str.find(' ') != std::string::npos)
			return "\"" + str + "\"";
		else
			return str;
	}

	size_t ParsingState::currentPosition() const {
		size_t position = 0;
		for (size_t i = 0; i < current_word; ++i) position += withQuotes(words[i]).size() + 1;

		if (current_word < words.size()) {
			// We add 1 if the current word requires quotes.
			position += (withQuotes(words[current_word]).size() - words[current_word].size()) / 2;

			position += inside_word_position;
		}
		return position;
	}

	std::string ParsingState::mergedArguments() const {
		std::string args;
		for (size_t i = 0; i < this->words.size(); ++i) {
			args += withQuotes(this->words[i]);
			if (i + 1 < this->words.size()) args += ' ';
		}
		return args;
	}
}
