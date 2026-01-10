#pragma once

#include <string>
#include "filesystem/file.hpp"


namespace lsp {
	std::string getDiagnosticFromCompiler(fs::File& file);
}