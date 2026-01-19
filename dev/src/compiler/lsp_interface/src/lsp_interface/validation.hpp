#pragma once

#include <filesystem/file.hpp>

#include <string>

namespace lsp {
	/**
	 * @brief The main function to get the JSON serialized diagnostics from the compiler.
	 *
	 * @warning It compiles the entire package the file belongs to, so also other modules
	 * unrelated to the file.
	 *
	 * It performs the following steps:
	 * 1. Finds the the module of the @param file and its package.
	 * 2. Collects the parser errors from the entire package.
	 * 3. Runs HELIOS compilation on the package to get the semantic diagnostics.
	 * 4. Serializes all the diagnostics to JSON format compatible with LSP interface.
	 *
	 * @param file The file to get diagnostics for.
	 *
	 * @return std::string The JSON serialized
	 */
	std::string getDiagnosticJsonFromCompiler(const fs::File& file);
}
