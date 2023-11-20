/**
 * @file clap.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "clap.hpp"
#include "param_builder.hpp"
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

		if (str[position] == '=')
			position++;
		else
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

	base::Optional<clap::ClapParameter&> getParam(
		clap::ParsingResult&                    result,
		const std::vector<clap::ClapParameter>& parameters,
		NameType                                name_type,
		const std::string&                      param_name
	) {
		for (auto& param: parameters) {
			if (name_type == NameType::ShortName) {
				if_opt_some(param.getShortName(), val) {
					if (val == param_name[flag_pack_index]) return param;
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

					// Throw if duplicated
					if (result.hasParam(param))
						throw clap::exceptions::DuplicatedParameter(param_name);

					match_optional(PARSE(param.getValueParser())) {
						opt_some(value) result.insertParameterValue(param, value);
						opt_none throw exceptions::ParameterRequiresValue(
							param_name, param.getValueParser()->getTypeName()
						);
					}
				}
				if (is_flag_pack && flag_pack_index + 1 < param_name.size()) {
					found_param = false;
					flag_pack_index++;
				}
				break;
			}
		}
	}
}

namespace clap {
	Clap& Clap::add(ClapParameter&& parameter) {
		parameters.push_back(std::move(parameter));
		return *this;
	}

	ParsingResult Clap::parse(CLIArgs args) { return parse(args.argc, args.argv); }

	ParsingResult Clap::parse(usize argc, const char** argv) {
		std::string args = mergeArgs(argc, argv);

		usize path_offset = 0;
		// Check if program is invoked using "./" or by name. This is potentially unsafe.
		if (argv[0][0] == '.' && argv[0][1] == '/') path_offset = 2;

		ParsingResult result(argv[0] + path_offset, args);

		// Going left to right through chars in args.
		usize parsing_position = 0;
		skipWhitespace(parsing_position, args);

		while (parsing_position < args.size()) {
			// If not found a "-" parse using default value parser
			if (args[parsing_position] == '-') {
				// It could be a negative number, like -1, or -.5
				bool is_number
					= (parsing_position + 1 < args.size()
				       && (args[parsing_position + 1] == '.'
				           || std::isdigit(args[parsing_position + 1])));
				if (!is_number) {
					auto [param_name, name_type] = parseParamName(parsing_position, args);
					if (name_type == NameType::EmptyName)
						throw exceptions::ExpectedParameterIdentifier((i32) parsing_position, args);

					bool  found_param     = false;
					usize flag_pack_index = 0;

					// A flag pack is multiple flags after one "-" like "tar -xf file" where "-xf"
					// is a flag pack.
					bool is_flag_pack = name_type == NameType::ShortName && param_name.size() > 1;

					auto paramOpt = getParam(result, parameters, name_type, param_name);

					if (paramOpt.has_value())
						continue;
					else
						throw exceptions::InvalidParameterName(param_name);
				}
			}
			// Check if value is positional or extra.
			usize current_positional_args = result.getPositionalParameterCount();
			if (current_positional_args < getPositionalParameters().size()) {
				const auto& param = getPositionalParameters()[current_positional_args];
				match_optional(PARSE(param.get())) {
					opt_some(value) result.insertPositional(value);
					opt_none throw exceptions::ClapException(
						"Cannot continue parsing... Please report this incident."
					);
				}
			} else {
				// So it's an extra argument.
				match_optional(PARSE(default_value_parser.get())) {
					opt_some(value) result.insertExtra(value);
					opt_none throw exceptions::ClapException(
						"Cannot continue parsing... Please report this incident."
					);
				}
			}
		}

		if (result.isFlag("help")) throw exceptions::HelpException(result);

		validateParsing(result);

		return result;
	}

	const ValueParser* Clap::getDefaultValueParser() const { return default_value_parser.get(); }

	const std::vector<ClapParameter>& Clap::getParameters() const { return parameters; }

	Clap& Clap::addPositional(base::unique_ptr<ValueParser> parameter) {
		positional_parameters.push_back(std::move(parameter));
		return *this;
	}

	Clap& Clap::setDefaultParser(base::unique_ptr<ValueParser> parser) {
		default_value_parser = std::move(parser);
		return *this;
	}

	void Clap::validateParsing(ParsingResult& result) const {
		usize num_positional_args = result.getPositionalParameterCount();

		if (num_positional_args < getPositionalParameters().size()) {
			const auto& param = getPositionalParameters()[num_positional_args];
			throw exceptions::PositionalParameterExpected(
				result.getPositionalParameterCount(), param->getTypeName()
			);
		}

		for (auto& param: parameters) {
			variant_match(param.getParameterNecessity()) {
				variant_case(Required, _) {
					if (!result.hasParam(param))
						throw exceptions::MissingRequiredParameter("\"" + getName(param) + "\"");
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

	Clap::Clap() { default_value_parser = StringParser::make(); }

	Clap& Clap::addHelpFlag() {
		return add(ParamBuilder::ofFlag()
		               .addShortName('h')
		               .addLongName("help")
		               .addShortDesc("Display this information.")
		               .build());
	}

	const std::vector<base::unique_ptr<ValueParser>>& Clap::getPositionalParameters() const {
		return positional_parameters;
	}


}  // clap
