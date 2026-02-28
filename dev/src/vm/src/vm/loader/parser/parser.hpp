#pragma once

#include "elements.hpp"

namespace vm::loader::parser {
	/**
	 * @brief Parses a list of files and returns dia_int::Logger with errors upon failure.
	 */
	std::expected<std::vector<ParsedFile>, dia_int::Logger> parse(const std::vector<fs::File>& file);
}
