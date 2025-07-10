/**
 * @file clap_class.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "clap.hpp"
#include "clap/command.hpp"
#include "exceptions.hpp"
#include "param_builder.hpp"
#include "value_parser.hpp"

#include <base/box.hpp>
#include <base/str_utils.hpp>
#include <base/variant.hpp>

#include <cctype>
#include <exception>
#include <iostream>
#include <utility>

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
				return { { .value = parsed.value, .raw_source = parsed.raw_source } };
			}
			return {};
		}
	};
}

namespace clap {
	Clap::Clap(std::string name, std::string description):
		  root_command(Command(std::move(name), std::move(description))) {}

	Clap&& Clap::addSubcommand(Command&& sub_command) {
		root_command.addSubcommand(std::move(sub_command));
		return std::move(*this);
	}

	Clap&& Clap::addGlobalParameter(Parameter&& parameter) {
		root_command.add(std::move(parameter));
		return std::move(*this);
	}

	Clap&& Clap::setPreHandler(PreHandler handler) {
		pre_handler = std::move(handler);
		return std::move(*this);
	}

	Clap&& Clap::setDefaultValueParser(MBox<ValueParser> parser) {
		root_command.setDefaultValueParser(std::move(parser));
		return std::move(*this);
	}

	const std::vector<Command>& Clap::getSubcommands() const {
		return root_command.getSubcommands();
	}

	const std::vector<Parameter>& Clap::getGlobalParameters() const {
		return root_command.getParameters();
	}

	MCRef<ValueParser> Clap::getDefaultValueParser() const {
		return root_command.getDefaultValueParser();
	}

	const Clap::PreHandler& Clap::getPreHandler() const { return pre_handler; }

	const Command& Clap::getRootCommand() const { return root_command; }

	Clap&& Clap::addHelpFlag() {
		return addGlobalParameter(
			ParamBuilder::ofFlag()
				.addShortName('h')
				.addLongName("help")
				.addShortDesc("Display this information.")
				.build()
		);
	}

	int Clap::execute(int argc, const char* const* argv) {
		try {
			// Returns a ParsingResult and a command that matched.
			// TODOP: Move command into parsing result.
			auto [command, parsing_result] = parse(argc, argv);

			// Prehandler executes before every other functions. Sets global flags in modules etc.
			if (pre_handler) pre_handler(parsing_result);

			// TODOP: Command might not have been found.
			// if (command) {
			// Get a handler that handles that command.
			const auto& handler = command.getHandler();
			if (handler) return handler(parsing_result);
			// }
			// No handler available, print help.
			// printHelp();
			return 1;
		} catch (const exceptions::HelpException& e) {
			// printHelp();
			return 0;
		} catch (const exceptions::ClapException& e) {
			std::cerr << "Error: " << e.what() << '\n';
			return 1;
		} catch (const std::exception& e) {
			std::cerr << "Something went wrong. Non CLAP exception wa thrown\n";
			return 1;
		}
	}

	Clap::CommandAndArgs Clap::newParse(CLIArgs args) { return newParse(args.argc, args.argv); }

	Clap::CommandAndArgs Clap::newParse(usize argc, const char* const* argv) {
		// TODOP: Remove that.
		CORE_ASSERT(
			argc > 0,
			"clap assumes argc is at least 1, as it is the name of the program from the parameters."
		);

		ParsingState                st(argc, argv);
		const Command*              current_command = &root_command;
		std::vector<const Command*> path;
		path.push_back(current_command);


		// Find subcommands.
		bool looking_for_subcommands = true;
		while (looking_for_subcommands && st.parsing_position < st.args.size()) {
			usize       token_start = st.parsing_position;
			std::string token;

			// Parse a single token until the space.
			while (st.parsing_position < st.args.size()
			       && not std::isspace(st.args[st.parsing_position])) {
				token += st.args[st.parsing_position++];
			}

			// Is token is an option, then we finish looking for commands.
			if (token.empty() || token.starts_with('-')) {
				st.parsing_position     = token_start;
				looking_for_subcommands = false;
				break;
			}

			const Command* next_command = nullptr;
			// TODOP: Use a map.
			for (const auto& sub_cmd: current_command->getSubcommands()) {
				if (token == sub_cmd.getName()) {
					// Found a sub command.
					next_command = &sub_cmd;
					break;
				}
			}

			// Step deeper into the new command.
			if (next_command) {
				current_command = next_command;
				path.push_back(current_command);
				// Skip the space after the command name.
				skipWhitespace(st.parsing_position, st.args);
			} else {  // No new command found. A token was a positional argument. Go back.
				st.parsing_position     = token_start;
				looking_for_subcommands = false;
			}
		}

		// After we know the command, we can parse the arguments.
		// Parameters available for a command are it's parameters and the global ones.

		while (st.parsing_position < st.args.size()) {
			// It could be a negative number, like -1, or -.5
			bool is_negative_number = isNegativeNumber(st.parsing_position, st.args);

			if (st.args[st.parsing_position] == '-' && !is_negative_number) {
				st.parseParameter(current_command->getParameters());
			} else {
				// If not found a "-" parse using default value parser
				// Check if value is positional or extra.
				usize current_positional_args = st.result.getPositionalParameterCount();
				if (current_positional_args < current_command->getPositionalParameters().size()) {
					const auto& param
						= current_command->getPositionalParameters()[current_positional_args];
					st.parsePositional(*param);
				} else {
					// So it's an extra argument.
					auto parser = current_command->getDefaultValueParser();
					if (parser == nullptr)
						throw exceptions::NoDefaultValueParser((i32) st.parsing_position, st.args);
					st.parseExtra(*parser);
				}
			}
		}

		if (st.result.isFlag("help")) throw exceptions::HelpException(st.result);
		validateParsing(*current_command, st.result);

		return { *current_command, std::move(st.result) };
	}

	void Clap::validateParsing(const Command& command, ParsingResult& result) const {
		usize num_positional_args = result.getPositionalParameterCount();

		if (num_positional_args < command.getPositionalParameters().size()) {
			const auto& param = command.getPositionalParameters()[num_positional_args];
			throw exceptions::PositionalParameterExpected(
				result.getPositionalParameterCount(), param->getTypeName()
			);
		}

		// Check this command params.
		for (auto& param: command.getParameters()) {
			variant_match(param.getParameterNecessity()) {
				variant_case(Required, _) {
					if (!result.hasParam(param))
						throw exceptions::MissingRequiredParameter(
							"\"" + getParameterName(param) + "\""
						);
				}
				variant_case(Optional, _) { /* Nothing in this case */ }
				variant_case(Conditional, c) {
					if (!c.condition(result)) {
						throw exceptions::MissingConditionalParameter(
							getParameterName(param), "reason: " + c.condition_description
						);
					}
				}
			}
		}

		// Check global params.
		for (auto& param: root_command.getParameters()) {
			variant_match(param.getParameterNecessity()) {
				variant_case(Required, _) {
					if (!result.hasParam(param))
						throw exceptions::MissingRequiredParameter(
							"\"" + getParameterName(param) + "\""
						);
				}
				variant_case(Optional, _) { /* Nothing in this case */ }
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
}  // clap
