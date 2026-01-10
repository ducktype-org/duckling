#pragma once

#include "filesystem/file.hpp"

#include <string>

namespace lsp {
	/**
	 * @brief The main function to get the JSON serialized diagnostics from the compiler.
	 * It performs the following steps:
	 * 1. Collects the parser errors from the files belonging to the package of the argument file.
	 * 2. Runs HELIOS compilation on the package to get the semantic diagnostics.
	 * 3. Serializes all the diagnostics to JSON format.
	 *
	 * @param file The file to get diagnostics for. 
	 * Note that all the package related diagnostics will be returned. The file is used to identify the package.
	 * 
	 * @return std::string The JSON serialized
	 */
	std::string getDiagnosticJsonFromCompiler(fs::File& file);
}
