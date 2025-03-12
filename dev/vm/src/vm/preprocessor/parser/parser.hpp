#pragma once

#include "elements.hpp"

namespace vm::parser {
	std::expected<ParsedProgram, std::string> assemble(const std::vector<fs::FilePath>& files);
}
