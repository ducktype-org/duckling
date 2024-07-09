/**
 * @file compiler_config.hpp
 * @note This code is a legacy code, but is left for adaptation
 * to "global-compiler options" and future compiler handler
 */

#pragma once

#include <vector>
#include <string>
#include <printer/printer_content.hpp>
#include <clap/clap.hpp>
#include "filesystem/file.hpp"

namespace compiler {

	struct CompilerConfig {
		std::vector<fs::FilePath> files;

		bool        was_output = false;
		std::string output;
	};

	CompilerConfig fromArgs(clap::CLIArgs args);

	// clang-format off
	// @FIXME: this does not pass CI.
	printer::PrinterContentsSeq generateHelpMessage(const clap::exceptions::HelpException& e);
	//clang-format on

}
