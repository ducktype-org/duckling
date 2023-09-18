#pragma once

#include <config/cli_args.hpp>
#include <printer/printer.hpp>
#include <string>
#include <vector>

namespace compiler {

	struct CompilerConfig {
		bool                     was_help = false;
		std::vector<std::string> file_names;

		bool                     was_output = false;
		std::string              output;
	};

	CompilerConfig   fromArgs(config::CLIArgs args);
	printer::Message generateHelpMessage();

}
