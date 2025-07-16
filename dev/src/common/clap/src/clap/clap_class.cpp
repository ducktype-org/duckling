/**
 * @file clap_class.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "clap.hpp"
#include "clap/command.hpp"
#include "clap/help_message_generator.hpp"
#include "clap/parameter.hpp"
#include "clap/parsing_result.hpp"
#include "clap/parsing_state.hpp"
#include "exceptions.hpp"
#include "param_builder.hpp"
#include "value_parser.hpp"

#include "base/optional.hpp"
#include <base/box.hpp>
#include <base/str_utils.hpp>
#include <base/variant.hpp>

#include <iostream>
#include <utility>

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

	Clap&& Clap::addPositional(Box<ValueParser> parser) {
		root_command.addPositional(std::move(parser));
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

	void Clap::dPrint() {
		// Helper lambda to print a command and its subcommands recursively
		std::function<void(const Command&, int)> print_command;
		print_command = [&](const Command& cmd, int indent) {
			std::string ind(usize(2 * indent), ' ');
			std::cout << ind << "Command: " << cmd.getName() << "\n";
			std::cout << ind << "  Description: " << cmd.getDescription() << "\n";

			// Print parameters
			if (!cmd.getParameters().empty()) {
				std::cout << ind << "  Parameters:\n";
				for (const auto& param: cmd.getParameters()) {
					std::cout << ind << "    - ";
					if (param.getShortName().has_value())
						std::cout << "--" << param.getShortName().value() << " ";
					if (param.getLongName().has_value())
						std::cout << "-" << param.getLongName()->stdString() << " ";
					std::cout << ": " << param.getShortDesc().stdString() << "\n";
				}
			}

			// Print positional parameters
			if (!cmd.getPositionalParameters().empty()) {
				std::cout << ind << "  Positional parameters:\n";
				for (size_t i = 0; i < cmd.getPositionalParameters().size(); ++i) {
					auto& parser = cmd.getPositionalParameters()[i];
					std::cout << ind << "    [" << i << "]: " << parser->getTypeName() << "\n";
				}
			}

			// Print subcommands recursively
			if (!cmd.getSubcommands().empty()) {
				std::cout << ind << "  Subcommands:\n";
				for (const auto& sub: cmd.getSubcommands()) print_command(sub, indent + 2);
			}
		};

		std::cout << "CLAP structure dump:\n";
		print_command(root_command, 0);
	}

	ParsingResult Clap::parse(CLIArgs args) { return parse(args.argc, args.argv); }

	// TODOP: Initializes the parsing.
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
		root_command.parse(st, root_command);
		validateParsing(st.result);
		return std::move(st.result);
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
		// Validate global params.
		if (command.get() != &root_command) validate_parameters(root_command.getParameters());
	}
}  // clap
