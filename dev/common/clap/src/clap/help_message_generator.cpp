/**
 * @file help_message_generator.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "help_message_generator.hpp"
#include "base/variant.hpp"

namespace {
	std::string getFileName(const std::string& path) {
		usize position = path.size() - 1;
		while (position > 0 && path[position] != '/' && path[position] != '\\') position--;
		return path.substr(position, path.size() - position + 1);
	}

	std::string generateUsage(const clap::Clap& clap, const clap::ParsingResult& parsing_result) {
		std::string usage = "Usage: " + getFileName(parsing_result.getFilePath()) + " ";

		for (const auto& positional: clap.getPositionalParameters())
			usage += "<" + positional->getTypeName() + "> ";

		for (const auto& param: clap.getParameters()) {
			variant_match(param.getParameterNecessity()) {
				variant_case(clap::Required, _) {
					usage += "<";
					bool has_short_name = false;
					if_opt_some(param.getShortName(), name) {
						usage += "-" + std::string(1, name);
						has_short_name = true;
					}
					if_opt_some(param.getLongName(), name) {
						if (has_short_name) usage += "/";
						usage += "--" + name.stdString();
					}
					usage += " " + param.getValueParser()->getTypeName() + "> ";
				}
			}
		}

		usage += "[options] ";
		usage += "[" + clap.getDefaultValueParser()->getTypeName() + "...]";
		usage += '\n';

		return usage;
	}

	std::string generateOptions(const clap::Clap& clap, int padding = 27) {
		std::stringstream output;

		output << "Options:\n";
		for (const auto& param: clap.getParameters()) {
			std::string names          = "  ";
			bool        has_short_name = false;
			if_opt_some(param.getShortName(), name) {
				names += "-";
				names += name;
				has_short_name = true;
			}
			if_opt_some(param.getLongName(), name) {
				if (has_short_name) names += ", ";
				names += "--";
				names += name.stdString();
			}
			if (param.getValueParser() != nullptr)
				names += " <" + param.getValueParser()->getTypeName() + ">";

			output << std::setw(padding) << std::left;
			output << names + " " << param.getShortDesc().stdString() << '\n';
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
}

namespace clap {
	std::string
		HelpMessageGenerator::generate(const Clap& clap, const ParsingResult& parsing_result) {
		std::string program_name = getFileName(parsing_result.getFilePath());

		std::string output;
		output += generateUsage(clap, parsing_result);
		if (!clap.getParameters().empty()) output += generateOptions(clap);

		return output;
	}
}  // clap
