/**
 * @file help_message_generator.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "help_message_generator.hpp"

#include "clap/command.hpp"
#include "clap/parameter.hpp"

#include "base/optional.hpp"
#include <base/variant.hpp>

#include <cstddef>
#include <iomanip>
#include <ios>
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
	 * @param program_name The name of the executable.
	 * @param command_path The path from the root command to the current one.
	 * @param command The specific command for which help is generated.
	 * @return A formatted "Usage" string.
	 */
	std::string generateUsage(
		const std::string&                       program_name,
		const std::vector<const clap::Command*>& command_path,
		const clap::Command&                     command
	) {
		std::stringstream usage;
		usage << "Usage" << program_name;
		// Add a path to this command.
		for (const auto& cmd: command_path | std::views::drop(1)) usage << " " << cmd->getName();

		if (not command.getSubcommands().empty()) {
			usage << " <COMMAND> [OPTIONS]";
			return usage.str();
		}

		for (const auto& param: command.getParameters()) {
			variant_match(param.getParameterNecessity()) {
				variant_case(clap::Required, _) {
					usage << " ";
					bool has_long_name = param.getLongName().has_value();
					// If a log name is available, we prefer it.
					if (has_long_name)
						usage << "--" << param.getLongName()->stdString();
					else if (param.getShortName().has_value())
						usage << "-" << param.getShortName().value();

					if (param.getValueParser() != nullptr)
						usage << " < " << param.getValueParser()->getTypeName() << ">";
				}
			}
		}
		for (const auto& positional: command.getPositionalParameters())
			usage << " <" << positional->getTypeName() << ">";
		usage << " [OPTIONS]";
		auto default_parser = command.getDefaultValueParser();
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
			output << '\n';
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
	std::string HelpMessageGenerator::generate(
		const Clap&                        clap,
		const Command&                     command,
		const std::vector<const Command*>& command_path,
		const std::string&                 program_name
	) {
		std::stringstream output;
		if (not command.getDescription().empty()) output << command.getDescription() << "\n\n";
		output << generateUsage(program_name, command_path, command) << '\n';
		output << generateOptionsBlock("Options:", command.getParameters());
		output << generateSubcommandsBlock(command.getSubcommands());

		if (&command != &clap.getRootCommand()) {  // TODOP: Write a comparision operator.
			output << generateOptionsBlock("Global Options:", clap.getRootCommand().getParameters());
		}

		if (not command.getSubcommands().empty()) {
			output << "\nRun '" << program_name
				   << " <COMMAND> --help for more information on a command.";
		}
		return output.str();
	}
}  // clap
