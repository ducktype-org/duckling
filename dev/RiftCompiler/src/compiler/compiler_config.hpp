#pragma once

#include <vector>
#include <string>
#include <printer/printer.hpp>
#include <config/cli_args.hpp>

namespace compiler {

	struct CompilerConfig {
		bool                     was_help = false;
		std::vector<std::string> file_names;

		bool        was_output = false;
		std::string output;
	};

	CompilerConfig   fromArgs(config::CLIArgs args);
	printer::Message generateHelpMessage();

}
