/**
 * @file clap_class.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "clap.hpp"

#include "base/optional.hpp"
#include <base/box.hpp>
#include <base/str_utils.hpp>
#include <base/variant.hpp>

#include <iostream>
#include <utility>

/**
 * Basic helper functions.
 */
namespace {
	/**
	 * @brief Checks if a token represents a negative number.
	 * @param token The token to check.
	 * @return True if it's a negative number.
	 */
	bool isNegativeNumber(const std::string& token) {
		if (token.empty() || token[0] != '-') return false;
		if (token.length() == 1) return false;

		// Check if all of the chars are digits and contain at most one dot.
		bool has_dot = false;
		for (usize i = 1; i < token.length(); ++i) {
			if (!std::isdigit(token[i])) {
				if (token[i] == '.' && !has_dot)
					has_dot = true;
				else
					return false;
			}
		}
		return true;
	}

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
	Clap::Clap(std::string name, std::string description):
		  name(std::move(name)),
		  description(std::move(description)),
		  is_leaf(true),
		  is_root(true),
		  default_value_parser(StringParser::make()) {}

	Clap&& Clap::add(Parameter&& param) {
		parameters.push_back(std::move(param));
		return std::move(*this);
	}

	Clap&& Clap::addPositional(Box<ValueParser> parser) {
		if (!is_leaf) throw clap::exceptions::CoexistingPositionalAndSubcommand(name);
		positional_parameters.push_back(std::move(parser));
		return std::move(*this);
	}

	Clap&& Clap::addSubcommand(Clap&& sub_command) {
		if (!positional_parameters.empty())
			throw clap::exceptions::CoexistingPositionalAndSubcommand(name);

		// Check for duplicates.
		for (const auto& subcmd: subcommands)
			if (subcmd.getName() == sub_command.getName())
				throw clap::exceptions::DuplicateSubcommand(sub_command.getName(), name);

		sub_command.is_root = false;
		is_leaf             = false;
		subcommands.emplace_back(std::move(sub_command));
		return std::move(*this);
	}

	Clap&& Clap::setHandler(Handler handl) {
		handler = std::move(handl);
		return std::move(*this);
	}

	Clap&& Clap::setPreHandler(PreHandler handler) {
		pre_handler = std::move(handler);
		return std::move(*this);
	}

	Clap&& Clap::setDefaultValueParser(MBox<ValueParser> parser) {
		default_value_parser = std::move(parser);
		return std::move(*this);
	}

	Clap&& Clap::addHelpFlag() {
		return add(
			ParamBuilder::ofFlag()
				.addShortName('h')
				.addLongName("help")
				.addShortDesc("Display this information.")
				.build()
		);
	}

	const std::string& Clap::getName() const { return name; }

	const std::string& Clap::getDescription() const { return description; }

	MCRef<ValueParser> Clap::getDefaultValueParser() const { return default_value_parser.ref(); }

	const Clap::PreHandler& Clap::getPreHandler() const { return pre_handler; }

	const Clap::Handler& Clap::getHandler() const { return handler; }

	const std::vector<Parameter>& Clap::getParameters() const { return parameters; }

	const std::vector<Box<ValueParser>>& Clap::getPositionalParameters() const {
		return positional_parameters;
	}

	const std::vector<Clap>& Clap::getSubcommands() const { return subcommands; }

	base::Optional<CRef<Clap>> Clap::getSubcommand(const std::string& subcommand_name) const {
		for (const auto& cmd: subcommands)
			if (cmd.getName() == subcommand_name) return &cmd;
		return {};
	}

	bool Clap::isLeaf() const { return is_leaf; }

	bool Clap::isRoot() const { return is_root; }

	ParsingResult Clap::parse(CLIArgs args) { return parse(args.argc, args.argv); }

	ParsingResult Clap::parse(usize argc, const char* const* argv) {
		// TODOP: Remove that.
		CORE_ASSERT(
			argc > 0,
			"clap assumes argc is at least 1, as it is the name of the program from the parameters."
		);

		// Initialize the parsing state.
		ParsingState st(argc, argv);

		// Perform the recursive parsing.
		// TODOP: This root command is not needed as a param I believe.
		parse(st);
		validateParsing(st.result);
		return std::move(st.result);
	}

	void Clap::parse(ParsingState& st) const {
		// Add `this` command to the result path.
		st.result.addToPath(this);

		while (st.hasMoreArgs()) {
			const std::string& token              = st.peekToken();
			bool               is_negative_number = isNegativeNumber(token);

			// Parameter
			if (token.starts_with('-') && !is_negative_number)
				st.parseParameter(parameters);
			else {  // Subcommand or positional.
				auto maybe_subcmd = getSubcommand(token);
				if_opt_some(maybe_subcmd, subcmd) {
					// It's a subcommand, go down the tree.
					st.consumeToken();
					subcmd->parse(st);
					return;
				}

				if (!is_leaf) {
					// If it's not subcommand (its a positional) and this subcommand is not a
					// leaf then we throw an error, since the subcommand is not specified.
					throw exceptions::SubcommandNotSpecified(name);
				}

				// If not found a "-" parse using default value parser.
				// Check if value is positional or extra.
				usize positional_count = st.result.getPositionalParameterCount();
				if (positional_count < getPositionalParameters().size()) {
					const auto& value_parser = getPositionalParameters()[positional_count];
					st.parsePositional(*value_parser);
				} else {                    // To many positional arguments.
					auto parser = getDefaultValueParser();
					if (parser == nullptr)  // Extra arguments and no default value parser.
						throw exceptions::NoDefaultValueParser((i32) st.parsing_position, st.args);
					st.parseExtra(*parser);
				}
			}
		}
		// The only way we get here is when all arguments where parsed.
		// This means `this` is the matched subcommand which has to be a leaf.
		if (!is_leaf) throw exceptions::SubcommandNotSpecified(name);
	}

	int Clap::execute(usize argc, const char* const* argv) {
		try {
			// Returns a ParsingResult and a command that matched.
			auto parsing_result = parse(argc, argv);

			// parsing_result.dPrint();
			// Prehandler executes before every other functions. Sets global flags in modules etc.
			if (pre_handler) pre_handler(parsing_result);

			// Get a handler that handles that command.
			const auto& maybe_command = parsing_result.getMatchedCommand();
			if_opt_some(maybe_command, command) {
				const auto& handler = command->getHandler();
				if (handler)
					return handler(parsing_result);
				else
					throw exceptions::NoHandlerSpecified(command->getName());
			}

			throw exceptions::ClapException("No of the commands matched!");
			// TODOP: Maybe print helps if there's a mistake?

			// No handler available, print help.
			// printHelp();
			return 1;
		} catch (const exceptions::HelpException& e) {
			std::cout << clap::HelpMessageGenerator::generate(*this, e.parsing_result);
			// TODOP: Maybe don't catch errors here.
			return 0;
		}
	}

	void Clap::validateParsing(ParsingResult& result) const {
		usize num_positional_args = result.getPositionalParameterCount();
		auto  maybe_command       = result.getMatchedCommand();
		if (!maybe_command.has_value())
			throw clap::exceptions::ClapException("No command matched!");
		auto command = maybe_command.value();

		if (num_positional_args < command->getPositionalParameters().size()) {
			const auto& param = command->getPositionalParameters()[num_positional_args];
			throw exceptions::PositionalParameterExpected(
				result.getPositionalParameterCount(), param->getTypeName()
			);
		}

		auto validate_parameters = [&](const std::vector<Parameter>& params) {
			for (auto& param: params) {
				auto maybe_param_name = param.getParameterName();
				if (!maybe_param_name.has_value())
					throw clap::exceptions::ClapException("Parameter has no name!");

				variant_match(param.getParameterNecessity()) {
					variant_case(Required, _) {
						if (!result.hasParam(param))
							throw exceptions::MissingRequiredParameter(
								"\"" + maybe_param_name.value() + "\""
							);
					}
					variant_case(Optional, _) {
						/* Nothing in this case */ }
						variant_case(Conditional, c) {
							if (!c.condition(result)) {
								throw exceptions::MissingConditionalParameter(
									maybe_param_name.value(), "reason: " + c.condition_description
								);
							}
						}
				}
			}
		};

		// Validate this command params.
		validate_parameters(command->getParameters());
		// Validate global params. This function is invoked from the root command, thus we compare
		// it with this.
		if (command.get() != this) validate_parameters(getParameters());
	}
}  // clap
