/**
 * @file help_message_generator.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "help_message_generator.hpp"

#include "clap/command.hpp"
#include "clap/parameter.hpp"
#include "clap/parsing_result.hpp"

#include "base/optional.hpp"
#include <base/variant.hpp>

#include <cstddef>
#include <iomanip>
#include <ios>
#include <iostream>
#include <ranges>
#include <sstream>
#include <string>
#include <vector>

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
	std::string generateUsage(const clap::ParsingResult& result) {
		auto              command = result.getMatchedCommand();
		std::stringstream usage;
		usage << "Usage: " << getFileName(result.getFilePath());

		// Add a path to this command.
		for (const auto& cmd: result.getCommandPath() | std::views::drop(1))
			usage << " " << cmd->getName();

		if (not command->getSubcommands().empty()) {
			usage << " <COMMAND> [OPTIONS]";
			return usage.str();
		}

		for (const auto& param: command->getParameters()) {
			variant_match(param.getParameterNecessity()) {
				variant_case(clap::Required, _) {
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
		for (const auto& positional: command->getPositionalParameters())
			usage << " <" << positional->getTypeName() << ">";
		usage << " [OPTIONS] ";
		auto default_parser = command->getDefaultValueParser();
		if (default_parser != nullptr) usage << "[" + default_parser->getTypeName() + "...]";
		return usage.str();
	}

}

std::string generateOptionsBlock(
	const std::string& title, const std::vector<clap::Parameter>& params, int padding = 27
) {
	if (params.empty()) return "";

	std::stringstream output;
	output << '\n' << title << '\n';
	for (const auto& param: params) {
		std::stringstream names_stream;
		bool              has_short_name = false;  // TODOP: This may be not needed.
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

std::string generateSubcommandsBlock(
	const std::vector<clap::Command>& subcommands, int padding = 27
) {
	if (subcommands.empty()) return "";
	std::stringstream output;

	output << "\nCommands:\n";
	for (const auto& cmd: subcommands) {
		std::string name = " " + cmd.getName();
		output << std::setw(padding) << std::left << name;
		output << cmd.getDescription() << '\n';
	}
	return output.str();
}

namespace clap {
	std::string HelpMessageGenerator::generate(const Clap& clap, const ParsingResult& result) {
		std::stringstream output;
		auto              command      = result.getMatchedCommand();
		auto              program_name = getFileName(result.getFilePath());

		output << generateUsage(result) << '\n';
		if (command.get() != &clap.getRootCommand()) {
			output << generateOptionsBlock("Global options:", clap.getRootCommand().getParameters());
		}
		output << generateOptionsBlock("Subcommand options:", command->getParameters());
		output << generateSubcommandsBlock(command->getSubcommands());


		if (not command->getSubcommands().empty()) {
			output << "\nRun '" << program_name
				   << " <COMMAND> --help for more information on a command.\n";
		}
		return output.str();
	}
}  // clap
