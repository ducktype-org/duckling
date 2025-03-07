#pragma once

#include "base/optional.hpp"
#include <preprocessor/parser/elements.hpp>
#include <code_data/program.hpp>
#include <diagnostic/logger.hpp>

namespace vm::validator {
	/**
	 * @brief Validates the program.
	 * Returns an empty optional in case of success and an error string on failure.
	 *
	 * @return base::Optional<std::string>
	 */
	base::Optional<std::string> verify(const parser::ParsedProgram& program);
}
