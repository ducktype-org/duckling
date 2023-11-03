/**
 * @file config_parameter.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "clap.hpp"
#include "base/variant.hpp"
#include <cctype>
#include <iostream>

namespace {
	void skip_whitespace(usize& position, const std::string& str) {
		while (std::isspace(str[position])) position++;
	}

	enum class NameType { InvalidName, ShortName, LongName };

	std::pair<std::string, NameType> parse_param_name(usize& position, const std::string& str) {
		std::string name;
		int         counter = 0;
		while (position < str.size() && str[position] == '-') {
			position++;
			counter++;
		}
		while (position < str.size() && !std::isspace(str[position]) && str[position] != '=')
			name += str[position++];
		if (str[position] == '=') position++;
		skip_whitespace(position, str);
		if (name.empty()) return { "", NameType::InvalidName };
		return { name, counter == 1 ? NameType::ShortName : NameType::LongName };
	}


}

namespace clap {
	Clap& Clap::add(ClapParameter&& parameter) {
		parameters.push_back(std::move(parameter));
		return *this;
	}

	ParsingResult Clap::parse(usize argc, char* const* argv) {
		// Merge args with spaces between.
		std::string args;
		for (usize i = 1; i < argc; i++) args += std::string(argv[i]) + " ";

		// Create result object.
		std::string   arg0 = std::string(argv[0]);
		ParsingResult result(arg0.substr(2, arg0.size() - 2), args);

		// Going left to right through chars in args.
		// First are the positional parameters.
		// If a positional parameter fails to parse, then a user must have passed an invalid
		// argument.
		usize parsing_position = 0;
		for (auto& param: positional_parameters) {
			ValueParsingResult parsed = param->parse(parsing_position, args.c_str());
			if (parsed.position > parsing_position) {
				parsing_position = parsed.position;
				result.insertPositional({ parsed.value, parsed.raw_source });
			} else {
				// @TODO: Error: positional parameter expected
				continue;
			}
			skip_whitespace(parsing_position, args);
		}

		while (parsing_position < args.size()) {
			// If not found a "-" parse using default value parser
			if (args[parsing_position] == '-') {
				auto [param_name, name_type] = parse_param_name(parsing_position, args);
				if (name_type == NameType::InvalidName) {
					// @TODO: Error: Invalid name
					std::cerr << "Invalid name\n";
				}
				for (auto& param: parameters) {
					bool is_this_param = false;
					if (name_type == NameType::ShortName) {
						if_opt_some(param.getShortName(), val) {
							if (val == param_name[0]) is_this_param = true;
						}
					} else {
						if_opt_some(param.getLongName(), val) {
							if (val == param_name.c_str()) is_this_param = true;
						}
					}
					if (is_this_param) {
						if (param.getValueParser() == nullptr) {
							// then it's a flag
							result.insertFlag(param);
						} else {
							// it's a parsable value
							ValueParsingResult parsed
								= param.getValueParser()->parse(parsing_position, args.c_str());
							if (parsed.position > parsing_position) {
								parsing_position = parsed.position;
								result.insertParameterValue(
									param, { parsed.value, parsed.raw_source }
								);
							} else {
								// @TODO: Error: positional parameter expected
								continue;
							}
						}
					}
				}
			} else {
				ValueParsingResult parsed
					= default_value_parser->parse(parsing_position, args.c_str());
				if (parsed.position > parsing_position) {
					parsing_position = parsed.position;
					result.insertPositional({ parsed.value, parsed.raw_source });
				} else {
					// @TODO: Error: positional parameter expected
					continue;
				}
			}
			skip_whitespace(parsing_position, args);
		}

		//		for(auto& param: parameters) {
		//			variant_match(param.getParameterNecessity()) {
		//				variant_case(Required, _) {
		//					if(!)
		//				}
		//				variant_case(Optional, _) {
		//					// Nothing in this case
		//				}
		//				variant_case(Conditional, c) {
		//
		//				}
		//
		//			}
		//		}

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

}  // clap
