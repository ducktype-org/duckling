/**
 * @file clap_class.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "clap.hpp"
#include "param_builder.hpp"
#include "exceptions.hpp"
#include <base/variant.hpp>
#include <base/str_utils.hpp>
#include <cctype>
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
		while (std::isspace(str[position])) position++;
	}

	/**
	 * Returns a name of the parameter. A parameter is required to have at least one name.
	 * @param param The parameter to get the name from.
	 * @return The name of the parameter.
	 */
	std::string getParameterName(const clap::Parameter& param) {
		if_opt_some(param.getLongName(), name) return name.stdString();

		// Brace initializer, because name is a char.
		if_opt_some(param.getShortName(), name) return { name };

		throw clap::exceptions::ClapException("Parameter has no name!");
	}

	/**
	 * When encountering "-" character in the input it might be a name or a number.
	 * Performs checks for numbers of form: "-1" "-.5"
	 * @param pos Position in the str.
	 * @param str A source of chars.
	 * @return True if it's a number, false otherwise.
	 */
	bool isNegativeNumber(usize pos, const std::string& str) {
		if (str[pos] != '-') return false;
		if (pos + 1 >= str.size()) return false;
		if (std::isdigit(str[pos + 1])) return true;
		if (pos + 2 >= str.size()) return false;
		if (str[pos + 1] == '.' && std::isdigit(str[pos + 2])) return true;
		return false;
	}
}

/**
 * ParsingState related.
 */
namespace {

	enum class NameType { EmptyName, ShortName, LongName };

	/**
	 * A helper class for the method clap::Clap::parse().
	 */
	class ParsingState {
	public:
		usize               parsing_position = 0;  /// Position in the args.
		std::string         args;                  /// Merged arguments.
		clap::ParsingResult result;                /// The result of the parsing.

		ParsingState(usize argc, const char* const* argv) {
			args = mergeArgs(argc, argv);
			skipWhitespace(parsing_position, args);

			// Check if program is invoked using "./" or by name. This is potentially unsafe.
			usize path_offset = 0;
			if (argv[0][0] == '.' && argv[0][1] == '/') path_offset = 2;

			result = clap::ParsingResult(argv[0] + path_offset, args);
		}

		/**
		 * Tries to perform parsing of a positional argument with a parser.
		 * @param parser The parser to be used.
		 */
		void parsePositional(const clap::ValueParser& parser) {
			match_optional(parseValueWithParser(parser)) {
				opt_some(value) result.insertPositional(value);
				opt_none throw clap::exceptions::ClapException(
					"Cannot continue parsing... Please report this incident."
				);
			}
		}

		/**
		 * Tires to perform parsing of an extra argument with a parser.
		 * @param parser The parser to be used.
		 */
		void parseExtra(const clap::ValueParser& parser) {
			match_optional(parseValueWithParser(parser)) {
				opt_some(value) result.insertExtra(value);
				opt_none throw clap::exceptions::ClapException(
					"Cannot continue parsing... Please report this incident."
				);
			}
		}

		/**
		 * Tries to perform parsing of a named parameter. It could be a flag or a value parameter.
		 * @param parameters All the available parameters.
		 */
		void parseParameter(const std::vector<clap::Parameter>& parameters) {
			auto [param_name, name_type] = parseName();
			if (name_type == NameType::EmptyName)
				throw clap::exceptions::ExpectedParameterIdentifier((i32) parsing_position, args);

			if (name_type == NameType::LongName)
				findParameterAndParse(parameters, param_name, name_type);
			else
				for (char c: param_name) findParameterAndParse(parameters, { c }, name_type);
		}

	private:
		/**
		 * Tries to parse a name of the parameter.
		 * @return The name and it's type: EmptyName, ShortName or LongName.
		 */
		std::pair<std::string, NameType> parseName() {
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

		/**
		 * Iterates over a collection of parameters and matches a name to the parameter,
		 * then performs parsing using it's ValueParser if provided.
		 * @param parameters All of the available parameters.
		 * @param param_name The parsed name.
		 * @param name_type Type of the parsed name.
		 */
		void findParameterAndParse(
			const std::vector<clap::Parameter>& parameters,
			const std::string&                  param_name,
			NameType                            name_type
		) {
			bool found_parameter = false;
			for (auto& parameter: parameters) {
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
					break;
				}
			}
			if (!found_parameter) throw clap::exceptions::InvalidParameterName(param_name);
		}

		/**
		 * Parses the value or inserts a flag to the ParsingResult if no ValueParer provided.
		 * @param parameter The parameter with mathing name.
		 * @param name The name of the parameter.
		 */
		void parseWithParameter(const clap::Parameter& parameter, const std::string& name) {
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

		/**
		 * Tries to perform parsing with a value parser. Returns an empty optional if no more
		 * characters are left.
		 * @param parser The parser to be used.
		 * @return An optionally parsed value.
		 */
		base::Optional<clap::ParsedValue> parseValueWithParser(const clap::ValueParser& parser) {
			clap::ValueParsingResult parsed = parser.parse(parsing_position, args);
			if (parsed.position > parsing_position) {
				parsing_position = parsed.position;
				skipWhitespace(parsing_position, args);
				return { { parsed.value, parsed.raw_source } };
			}
			return {};
		}
	};
}

namespace clap {
	Clap& Clap::add(Parameter&& parameter) {
		parameters.push_back(std::move(parameter));
		return *this;
	}

	ParsingResult Clap::parse(CLIArgs args) { return parse(args.argc, args.argv); }

	ParsingResult Clap::parse(usize argc, const char* const* argv) {
		CORE_ASSERT(
			argc > 0,
			"clap assumes argc is at least 1, as it is the name of the program from the parameters."
		);

		ParsingState st(argc, argv);

		// Going left to right through chars in args.
		while (st.parsing_position < st.args.size()) {
			// It could be a negative number, like -1, or -.5
			bool is_negative_number = isNegativeNumber(st.parsing_position, st.args);

			if (st.args[st.parsing_position] == '-' && !is_negative_number) {
				st.parseParameter(getParameters());
			} else {
				// If not found a "-" parse using default value parser
				// Check if value is positional or extra.
				usize current_positional_args = st.result.getPositionalParameterCount();
				if (current_positional_args < getPositionalParameters().size()) {
					const auto& param = getPositionalParameters()[current_positional_args];
					st.parsePositional(*param);
				} else {
					// So it's an extra argument.
					auto parser = getDefaultValueParser();
					if (parser == nullptr)
						throw exceptions::NoDefaultValueParser((i32) st.parsing_position, st.args);
					st.parseExtra(*parser);
				}
			}
		}

		if (st.result.isFlag("help")) throw exceptions::HelpException(st.result);
		validateParsing(st.result);

		return st.result;
	}

	base::borrow_ptr<const ValueParser> Clap::getDefaultValueParser() const {
		return base::borrow_ptr(default_value_parser.get());
	}

	const std::vector<Parameter>& Clap::getParameters() const { return parameters; }

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
						throw exceptions::MissingRequiredParameter(
							"\"" + getParameterName(param) + "\""
						);
				}
				variant_case(Optional, _) { /* Nothing in this case */
				}
				variant_case(Conditional, c) {
					if (!c.condition(result)) {
						throw exceptions::MissingConditionalParameter(
							getParameterName(param), "reason: " + c.condition_description
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
