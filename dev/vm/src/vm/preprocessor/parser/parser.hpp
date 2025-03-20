#pragma once

#include "elements.hpp"

namespace vm::parser {
	std::expected<std::vector<ParsedFile>, std::string> parse(const std::vector<fs::FilePath>& file
	);
}
