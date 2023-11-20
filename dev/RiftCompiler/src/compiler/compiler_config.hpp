#pragma once

#include <vector>
#include <string>
#include <printer/printer.hpp>
#include <clap/exceptions.hpp>
#include <clap/clap.hpp>

namespace compiler {

	struct CompilerConfig {
		std::vector<std::string> file_names;

		bool        was_output = false;
		std::string output;
	};

	CompilerConfig   fromArgs(clap::CLIArgs args);
	printer::Message generateHelpMessage(const clap::exceptions::HelpException& e);

}
