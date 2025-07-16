#include "parsing_state.hpp"

#include "clap/exceptions.hpp"

#include "base/ints.hpp"

#include <cctype>

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
	std::string mergeArgs(usize argc, const char* const* argv) {
		// Merge args with spaces between.
		std::string args;

		// if an argv[i] contains a white space, then it must have been added with quotes
		for (usize i = 1; i < argc; i++) {
			CORE_ASSERT(argv[i] != nullptr, "Clap received null pointer as one of argv arguments.");

			bool has_whitespace = false;
			auto arg            = std::string(argv[i]);
			for (auto c: arg)
				if (std::isspace(c)) has_whitespace = true;

			base::strReplaceAll(arg, "\"", "\\\"");

			if (has_whitespace)
				args += "\"" + arg + "\" ";
			else
				args += arg + " ";
		}
		// The last character is space, so we pop it.
		if (!args.empty()) args.pop_back();
		return args;
	}

	/**
	 * Moves the index in the string until a whitespace under the index.
	 * @param position A reference to the position's variable.
	 * @param str A source of chars.
	 */
	void skipWhitespace(usize& position, std::string_view str) {
		while (position < str.size() && std::isspace(str[position])) position++;
	}
}

namespace clap {
	ParsingState::ParsingState(usize argc, const char* const* argv) {
		args = mergeArgs(argc, argv);
		skipWhitespace(parsing_position, args);

		// Check if program is invoked using "./" or by name. This is potentially unsafe.
		usize path_offset = 0;
		if (argv[0][0] == '.' && argv[0][1] == '/') path_offset = 2;

		result = clap::ParsingResult(argv[0] + path_offset, args);
	}

	void ParsingState::parsePositional(const clap::ValueParser& parser) {
		match_optional(parseValueWithParser(parser)) {
			opt_some(value) result.insertPositional(value);
			opt_none throw clap::exceptions::ClapException(
				"Cannot continue parsing... Please report this incident."
			);
		}
	}

	void ParsingState::parseExtra(const clap::ValueParser& parser) {
		match_optional(parseValueWithParser(parser)) {
			opt_some(value) result.insertExtra(value);
			opt_none throw clap::exceptions::ClapException(
				"Cannot continue parsing... Please report this incident."
			);
		}
	}

	void ParsingState::parseParameter(const std::vector<clap::Parameter>& params) {
		auto [param_name, name_type] = parseName();
		if (name_type == NameType::EmptyName)
			throw clap::exceptions::ExpectedParameterIdentifier((i32) parsing_position, args);

		if (name_type == NameType::LongName) {
			if (!findParameterAndParse(params, param_name, name_type))
				throw clap::exceptions::InvalidParameterName(param_name);
		} else {
			for (char c: param_name)
				if (!findParameterAndParse(params, { c }, name_type))
					throw clap::exceptions::InvalidParameterName({ c });
		}
	}

	std::string ParsingState::peekToken() {
		if (!hasMoreArgs()) {
			throw exceptions::ClapException(
				"Parser error: peekToken() called with no more arguments"
			);
		}

		// Skip the whitespace's before the token.
		skipWhitespace(parsing_position, args);
		usize start_pos = parsing_position;
		usize end_pos   = start_pos;
		while (end_pos < args.size() && !std::isspace(args[end_pos])) end_pos++;
		return args.substr(start_pos, end_pos - start_pos);
	}

	bool ParsingState::hasMoreArgs() const { return parsing_position < args.size(); }

	void ParsingState::consumeToken() {
		if (!hasMoreArgs()) return;

		// Skip whitespace's before the token.
		skipWhitespace(parsing_position, args);
		while (parsing_position < args.size() && !std::isspace(args[parsing_position]))
			parsing_position++;
		// Skip whitespace's after the token.
		skipWhitespace(parsing_position, args);
	}

	std::pair<std::string, NameType> ParsingState::parseName() {
		std::string name;
		int         counter = 0;
		while (parsing_position < args.size() && args[parsing_position] == '-') {
			parsing_position++;
			counter++;
		}
		while (parsing_position < args.size() && !std::isspace(args[parsing_position])
		       && args[parsing_position] != '=')
			name += args[parsing_position++];

		if (args[parsing_position] == '=')
			parsing_position++;
		else
			skipWhitespace(parsing_position, args);

		if (name.empty()) return { "", NameType::EmptyName };
		return { name, counter == 1 ? NameType::ShortName : NameType::LongName };
	}

	bool ParsingState::findParameterAndParse(
		const std::vector<clap::Parameter>& parameters,
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

	void ParsingState::parseWithParameter(const clap::Parameter& parameter, const std::string& name) {
		if (parameter.getValueParser() == nullptr) {
			// then it's a flag
			result.insertFlag(parameter);
		} else {
			// Throw if duplicated
			if (result.hasParam(parameter)) throw clap::exceptions::DuplicatedParameter(name);

			match_optional(parseValueWithParser(*parameter.getValueParser())) {
				opt_some(parsed) result.insertParameterValue(parameter, parsed);
				opt_none throw clap::exceptions::ParameterRequiresValue(
					name, parameter.getValueParser()->getTypeName()
				);
			}
		}
	}

	base::Optional<clap::ParsedValue> ParsingState::parseValueWithParser(
		const clap::ValueParser& parser
	) {
		clap::ValueParsingResult parsed = parser.parse(parsing_position, args);
		if (parsed.position > parsing_position) {
			parsing_position = parsed.position;
			skipWhitespace(parsing_position, args);
			return { { .value = parsed.value, .raw_source = parsed.raw_source } };
		}
		return {};
	}


}
