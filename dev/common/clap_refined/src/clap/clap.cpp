/**
 * @file clap.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "clap.hpp"
#include "exceptions.hpp"
#include "base/variant.hpp"
#include <cctype>
#include <iostream>

#define PARSE(parser) parseWithParser(parsing_position, args, parser)

namespace {
	std::string mergeArgs(usize argc, const char** argv) {
		// Merge args with spaces between.
		std::string args;

		// if an argv[i] contains a white space, then it must have been added with quotes
		for (usize i = 1; i < argc; i++) {
			bool has_whitespace = false;
			auto arg            = std::string(argv[i]);
			for (auto c: arg)
				if (std::isspace(c)) has_whitespace = true;

			if (has_whitespace)
				args += "\"" + arg + "\" ";
			else
				args += arg + " ";
		}
		// The last character is space, so we pop it.
		if (!args.empty()) args.pop_back();
		return args;
	}

	void skipWhitespace(usize& position, std::string_view str) {
		while (std::isspace(str[position])) position++;
	}

	enum class NameType { EmptyName, ShortName, LongName };

	std::pair<std::string, NameType> parseParamName(usize& position, std::string_view str) {
		std::string name;
		int         counter = 0;
		while (position < str.size() && str[position] == '-') {
			position++;
			counter++;
		}
		while (position < str.size() && !std::isspace(str[position]) && str[position] != '=')
			name += str[position++];
		if (str[position] == '=') position++;
		skipWhitespace(position, str);
		if (name.empty()) return { "", NameType::EmptyName };
		return { name, counter == 1 ? NameType::ShortName : NameType::LongName };
	}

	base::Optional<clap::ParsedValue> parseWithParser(
		usize& parsing_position, std::string_view args, const clap::ValueParser* parser
	) {
		clap::ValueParsingResult parsed = parser->parse(parsing_position, args);
		if (parsed.position > parsing_position) {
			parsing_position = parsed.position;
			skipWhitespace(parsing_position, args);
			return { parsed.value, parsed.raw_source };
		} else {
			return {};
		}
	}

	std::string getName(const clap::ClapParameter& param) {
		if_opt_some(param.getLongName(), name) return name.stdString();
		if_opt_some(param.getShortName(), name) return { name };
		throw clap::exceptions::ClapException("Parameter has no name!");
	}
}

namespace clap {
	Clap& Clap::add(ClapParameter&& parameter) {
		parameters.push_back(std::move(parameter));
		return *this;
	}

	ParsingResult Clap::parse(usize argc, const char** argv) {
		std::string args = mergeArgs(argc, argv);

		ParsingResult result(argv[0] + 2, args);

		// Going left to right through chars in args.
		usize parsing_position = 0;
		skipWhitespace(parsing_position, args);

		// First are the positional parameters.
		// If a positional parameter fails to parse, then a user must have passed an invalid
		// argument.
		for (auto& param: positional_parameters) {
			base::Optional<ParsedValue> parsed = PARSE(param.get());
			match_optional(parsed) {
				opt_some(value) result.insertPositional(value);
				opt_none throw exceptions::PositionalParameterExpected(
					result.getPositionalParameterCount(), param->getTypeName()
				);
			}
		}

		while (parsing_position < args.size()) {
			// If not found a "-" parse using default value parser
			if (args[parsing_position] == '-') {
				// It could be a negative number, like -1, or -.5
				if (parsing_position + 1 < args.size()
				    && (args[parsing_position + 1] == '.' || std::isdigit(args[parsing_position])
				    )) {
					// It is a number most likely, so we skip this iteration and allow
					// default parser to work
					parsing_position++;
					continue;
				}
				auto [param_name, name_type] = parseParamName(parsing_position, args);
				if (name_type == NameType::EmptyName)
					throw exceptions::ExpectedParameterIdentifier((i32) parsing_position, args);
				bool found_param = false;
				for (auto& param: parameters) {
					if (name_type == NameType::ShortName) {
						if_opt_some(param.getShortName(), val) {
							if (val == param_name[0]) found_param = true;
						}
					} else {
						if_opt_some(param.getLongName(), val) {
							if (val == param_name.c_str()) found_param = true;
						}
					}
					if (found_param) {
						if (param.getValueParser() == nullptr) {
							// then it's a flag
							result.insertFlag(param);
						} else {
							// it's a parsable value
							base::Optional<ParsedValue> parsed = PARSE(param.getValueParser());
							match_optional(parsed) {
								opt_some(value) result.insertParameterValue(param, value);
								opt_none throw exceptions::ParameterRequiresValue(
									param_name, param.getValueParser()->getTypeName()
								);
							}
						}
						break;
					}
				}
				if (!found_param) throw exceptions::InvalidParameterName(param_name);
			} else {
				base::Optional<ParsedValue> parsed = PARSE(default_value_parser.get());
				match_optional(parsed) {
					opt_some(value) result.insertExtra(value);
					opt_none throw exceptions::ClapException(
						"Cannot continue parsing... Please report this incident"
					);
				}
			}
			skipWhitespace(parsing_position, args);
		}

		validate_parsing(result);

		return result;
	}

	const ValueParser* Clap::getDefaultValueParser() const { return default_value_parser.get(); }

	const std::vector<ClapParameter>& Clap::getParameters() const { return parameters; }

	Clap& Clap::addPositional(base::unique_ptr<ValueParser> parameter) {
		if (!parameters.empty())
			throw base::LogicError("Cannot add positional after a keyword parameter has been added"
			);
		positional_parameters.push_back(std::move(parameter));
		return *this;
	}

	Clap& Clap::setDefaultParser(base::unique_ptr<ValueParser> parser) {
		default_value_parser = std::move(parser);
		return *this;
	}

	void Clap::validate_parsing(ParsingResult& result) const {
		for (auto& param: parameters) {
			variant_match(param.getParameterNecessity()) {
				variant_case(Required, _) {
					if (!result.hasParam(param))
						throw exceptions::MissingRequiredParameter(getName(param));
				}
				variant_case(Optional, _) {
					// Nothing in this case
				}
				variant_case(Conditional, c) {
					if (!c.condition(result)) {
						throw exceptions::MissingConditionalParameter(
							getName(param),
							"A condition has not been met: " + c.condition_description
						);
					}
				}
			}
		}
	}

}  // clap
