/**
 * @file help_message_generator.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "help_message_generator.hpp"

#include <base/extend_cpp/variant_match.hpp>

#include <ranges>
#include <sstream>

namespace {
	std::string getFileName(const std::string& path) {
		auto position = path.find_last_of("/\\");
		if (position != std::string::npos) return path.substr(position + 1);
		return path;
	}

	/**
	 * @brief Generates "Usage" based on the command path.
	 * @param result The result of parsing.
	 * @return A formatted "Usage" string.
	 */
	std::string generateUsage(const clah::Clah& clah, const clah::ParsingResult& result) {
		auto              command = result.getMatchedCommand();
		std::stringstream usage;
		usage << "Usage: " << getFileName(result.getFilePath());

		auto print_required_parameters = [&](const std::vector<clah::Parameter>& parameters) {
			for (const auto& param: parameters) {
				variant_match(param.getParameterNecessity()) {
					variant_case(clah::Required, _) {
						usage << " ";
						bool has_long_name = param.getLongName().has_value();
						// If a long name is available, we prefer it.
						if (has_long_name)
							usage << "--" << param.getLongName()->stdString();
						else if (param.getShortName().has_value())
							usage << "-" << param.getShortName().value();

						if (param.getValueParser() != nullptr)
							usage << " <" << param.getValueParser()->getTypeName() << ">";
					}
				}
			}
		};

		// Print out the required global parameters.
		print_required_parameters(clah.getParameters());
		usage << " [GLOBAL OPTIONS]";

		// Add a path to this command.
		for (const auto& cmd: result.getCommandPath() | std::views::drop(1))
			usage << " " << cmd->getName();

		if (not command.value()->getSubcommands().empty()) {
			usage << " <COMMAND> [OPTIONS]";
			return usage.str();
		}

		// Print out the required global parameters.
		print_required_parameters(command.value()->getParameters());

		for (const auto& positional: command.value()->getPositionalParameters())
			usage << " <" << positional->getTypeName() << ">";
		usage << " [OPTIONS] ";
		auto default_parser = command.value()->getDefaultValueParser();
		if (default_parser != nullptr) usage << "[" + default_parser->getTypeName() + "...]";
		return usage.str();
	}

}

std::string generateOptionsBlock(
	const std::string& title, const std::vector<clah::Parameter>& params, int padding = 27
) {
	if (params.empty()) return "";

	std::stringstream output;
	output << '\n' << title << '\n';
	for (const auto& param: params) {
		std::stringstream names_stream;
		bool              has_short_name = false;
		names_stream << "  ";

		if_opt_some(param.getShortName(), name) {
			names_stream << "-" << name;
			has_short_name = true;
		}
		if_opt_some(param.getLongName(), name) {
			if (has_short_name) names_stream << ", ";
			names_stream << "--" << name.stdString();
		}
		if (param.getValueParser() != nullptr)
			names_stream << " <" + param.getValueParser()->getTypeName() + ">";
		output << std::setw(padding) << std::left << names_stream.str();
		output << param.getShortDesc().stdString() << '\n';

		if_opt_some(param.getLongDesc(), long_desc) {
			std::stringstream input(long_desc.stdString());
			std::string       line;
			while (std::getline(input, line))
				output << std::setw(padding) << std::left << " " << line << '\n';
		}
	}
	return output.str();
}

std::string generateSubcommandsBlock(const std::vector<clah::Clah>& subcommands, int padding = 27) {
	if (subcommands.empty()) return "";
	std::stringstream output;

	output << "\nAvailable Commands:\n";
	for (const auto& cmd: subcommands) {
		std::string name = " " + cmd.getName();
		output << std::setw(padding) << std::left << name;
		output << cmd.getDescription() << '\n';
	}
	return output.str();
}

namespace clah {
	std::string HelpMessageGenerator::generate(const Clah& clah, const ParsingResult& result) {
		std::stringstream output;
		auto              command      = result.getMatchedCommand();
		auto              program_name = getFileName(result.getFilePath());

		output << generateUsage(clah, result) << '\n';
		output << generateSubcommandsBlock(command.value()->getSubcommands());
		output << generateOptionsBlock("Global options:", clah.getParameters());
		if (command->get() != &clah)
			output << generateOptionsBlock("Command options:", command.value()->getParameters());


		if (not command.value()->getSubcommands().empty()) {
			output << "\nRun '" << program_name
				   << " <COMMAND> --help for more information on a command.\n";
		}
		return output.str();
	}
}  // clah
