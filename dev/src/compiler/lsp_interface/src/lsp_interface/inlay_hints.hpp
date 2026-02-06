#pragma once

#include <filesystem/file.hpp>


namespace lsp {
	std::string getInlayHintsJson(const fs::File& file);
}