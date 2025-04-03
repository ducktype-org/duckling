#pragma once

#include "elements.hpp"

namespace vm::parser {
	std::expected<std::vector<ParsedFile>, dia::Logger> parse(const std::vector<fs::FilePath>& file
	);
}
