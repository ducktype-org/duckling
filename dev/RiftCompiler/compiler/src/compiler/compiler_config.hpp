#pragma once

#include <vector>
#include <string>
#include <printer/message.hpp>
#include <clap/clap.hpp>
#include "filesystem/file.hpp"

namespace compiler {

	struct CompilerConfig {
		std::vector<fs::FilePath> files;

		bool        was_output = false;
		std::string output;
	};

	CompilerConfig   fromArgs(clap::CLIArgs args);
	printer::Message generateHelpMessage(const clap::exceptions::HelpException& e);

}
