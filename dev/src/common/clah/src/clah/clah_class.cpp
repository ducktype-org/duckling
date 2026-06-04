/**
 * @file clah_class.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "clah.hpp"

#include <base/collections/optional.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/box.hpp>

#include <printer/stream_printer.hpp>

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
}

namespace clah {

	Clah::Clah(): name(""), description(""), default_value_parser(StringParser::make()) {
		// A --help flag is added by default.
		addHelpFlag();
	}

	Clah::Clah(std::string name, std::string description):
		  name(std::move(name)),
		  description(std::move(description)),
		  default_value_parser(StringParser::make()) {
		// A --help flag is added by default.
		addHelpFlag();
	}

	Clah&& Clah::add(Parameter&& param) {
		parameters.push_back(std::move(param));
		return std::move(*this);
	}

	Clah&& Clah::addPositional(Box<ValueParser> parser) {
		if (!subcommands.empty()) throw clah::exceptions::CoexistingPositionalAndSubcommand(name);
		positional_parameters.push_back(std::move(parser));
		return std::move(*this);
	}

	Clah&& Clah::addSubcommand(Clah&& sub_command) {
		if (!positional_parameters.empty())
			throw clah::exceptions::CoexistingPositionalAndSubcommand(name);
		if (sub_command.getName() == "") throw clah::exceptions::UnnamedSubcommand(name);

		// Check for duplicates.
		for (const auto& subcmd: subcommands)
			if (subcmd.getName() == sub_command.getName())
				throw clah::exceptions::DuplicateSubcommand(sub_command.getName(), name);

		subcommands.emplace_back(std::move(sub_command));
		return std::move(*this);
	}

	Clah&& Clah::setHandler(Handler handl) {
		handler = std::move(handl);
		return std::move(*this);
	}

	Clah&& Clah::setPreHandler(PreHandler handler) {
		pre_handler = std::move(handler);
		return std::move(*this);
	}

	Clah&& Clah::addCustomVerification(CustomVerification verification) {
		custom_verifications.push_back(std::move(verification));
		return std::move(*this);
	}

	Clah&& Clah::setDefaultValueParser(MBox<ValueParser> parser) {
		default_value_parser = std::move(parser);
		return std::move(*this);
	}

	Clah&& Clah::addHelpFlag() {
		return add(ParamBuilder::ofFlag()
		               .addShortName('h')
		               .addLongName("help")
		               .addShortDesc("Display this information.")
		               .build());
	}

	const std::string& Clah::getName() const { return name; }

	const std::string& Clah::getDescription() const { return description; }

	MCRef<ValueParser> Clah::getDefaultValueParser() const { return default_value_parser.ref(); }

	const Clah::PreHandler& Clah::getPreHandler() const { return pre_handler; }

	const Clah::Handler& Clah::getHandler() const { return handler; }

	const std::vector<Parameter>& Clah::getParameters() const { return parameters; }

	const std::vector<Box<ValueParser>>& Clah::getPositionalParameters() const {
		return positional_parameters;
	}

	const std::vector<Clah>& Clah::getSubcommands() const { return subcommands; }

	base::Optional<CRef<Clah>> Clah::getSubcommand(const std::string& subcommand_name) const {
		for (const auto& cmd: subcommands)
			if (cmd.getName() == subcommand_name) return &cmd;
		return {};
	}

	ParsingResult Clah::parse(usize argc, const char* const* argv) {
		CORE_ASSERT(
			argc > 0,
			"clah assumes argc is at least 1, as it is the name of the program from the parameters."
		);

		// Initialize the parsing state.
		ParsingState st(argc, argv);

		// Perform the recursive parsing.
		parse(st);
		validateParsing(st.result);
		return std::move(st.result);
	}

	ParsingResult Clah::parseArgs(const std::string& args) {
		ParsingState st(args);
		parse(st);
		validateParsing(st.result);
		return std::move(st.result);
	}

	void Clah::parse(ParsingState& st) const {
		// Add `this` command to the result path.
		st.result.addToCommandList(this);

		while (not st.isEnd()) {
			std::string token              = st.frontWord();
			bool        is_negative_number = isNegativeNumber(token);

			// Parameter
			if (token.starts_with('-') && !is_negative_number)
				st.parseParameter(parameters);
			else if (auto maybe_subcmd = getSubcommand(token)) {  // Subcommand or positional.
				auto subcmd = maybe_subcmd.value();
				if (st.result.isFlag("help")) throw exceptions::HelpException(st.result);
				// It's a subcommand, go down the tree.
				st.advanceWord();

				subcmd->parse(st);
				return;
			} else {
				// If not found a "-" parse using default value parser.
				// Check if value is positional or extra.
				usize positional_count = st.result.getPositionalParameterCount();
				if (positional_count < getPositionalParameters().size()) {
					const auto& value_parser = getPositionalParameters()[positional_count];
					st.parsePositional(*value_parser);
				} else {                    // To many positional arguments.
					auto parser = getDefaultValueParser();
					if (parser == nullptr)  // Extra arguments and no default value parser.
						throw exceptions::NoDefaultValueParser(
							st.position_in_merged, st.merged_view
						);
					st.parseExtra(*parser);
				}
			}
		}
		if (st.result.isFlag("help")) throw exceptions::HelpException(st.result);
	}

	int Clah::execute(usize argc, const char* const* argv) {
		return execute(
			[&] { return parse(argc, argv); },
			[&](const auto& e) {
				printer::StreamPrinter::print({
					{ "[Clah error]: ", printer::Color::Red },
					{ e.what(), printer::Color::Default },
					{ "\n", printer::Color::Default },
					{ "Use \"", printer::Color::Default },
					{ argv[0], printer::Color::Default },
					{ " --help\" for available options.\n", printer::Color::Default },
				});
			}
		);
	}

	int Clah::execute(const std::string& args) {
		return execute(
			[&] { return parseArgs(args); },
			[](const auto& e) {
				printer::StreamPrinter::print({
					{ "[Clah error]: ", printer::Color::Red },
					{ e.what(), printer::Color::Default },
					{ "\n", printer::Color::Default },
					{ "Use \"--help\" for available options.\n", printer::Color::Default },
				});
			}
		);
	}

	int Clah::execute(
		std::function<ParsingResult()>                                parse,
		std::function<void(const clah::exceptions::ClahException& e)> on_clah_exception
	) {
		try {
			auto parsing_result = parse();

			if (pre_handler) pre_handler(parsing_result);

			// Get a handler that handles the matched command.
			const auto& maybe_command = parsing_result.getMatchedCommand();
			if_opt_some(maybe_command, command) {
				if (!command->getSubcommands().empty())
					throw clah::exceptions::SubcommandNotSpecified(command->getName());
				const auto& handler = command->getHandler();
				if (handler)
					return handler(parsing_result);
				else
					throw exceptions::NoHandlerSpecified(command->getName());
			}

			throw exceptions::ClahException("None of the commands matched!");
		} catch (const exceptions::HelpException& e) {
			std::cout << clah::HelpMessageGenerator::generate(*this, e.parsing_result);
			return 0;
		} catch (const exceptions::SuccessExitException& e) {
			return 0;
		} catch (const clah::exceptions::ClahException& e) {
			on_clah_exception(e);
			return 1;
		}
	}

	void Clah::validateParsing(ParsingResult& result) const {
		usize num_positional_args = result.getPositionalParameterCount();
		auto  maybe_command       = result.getMatchedCommand();
		if (!maybe_command.has_value())
			throw clah::exceptions::ClahException("No command matched!");
		auto command = maybe_command.value();

		auto run_custom_verifications = [&](const Clah& clah) {
			for (const auto& verification: clah.custom_verifications) {
				auto verification_result = verification(result);
				if (not verification_result.has_value())
					throw exceptions::CustomVerificationFailed(verification_result.error());
			}
		};

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
					throw clah::exceptions::ClahException("Parameter has no name!");

				variant_match(param.getParameterNecessity()) {
					variant_case(Required, _) {
						if (!result.hasParam(param))
							throw exceptions::MissingRequiredParameter(
								"\"" + maybe_param_name.value() + "\""
							);
					}
					variant_case(Optional, _) { /* Nothing in this case */ }
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
		// Validate global params. This function is invoked from the root command, thus we
		// compare it with this.
		if (command.get() != this) validate_parameters(getParameters());

		run_custom_verifications(*command);
		if (command.get() != this) run_custom_verifications(*this);
	}
}  // clah
