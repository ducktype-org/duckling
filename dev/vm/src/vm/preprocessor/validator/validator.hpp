#pragma once

#include <vm/preprocessor/preprocessor.hpp>
#include <vm/code/code.hpp>
#include <base/optional.hpp>
#include <vm/preprocessor/parser/elements.hpp>

namespace vm::validator {
	/**
	 * @brief Validates the program.
	 * Returns an empty optional in case of success and an error string on failure.
	 *
	 * @return base::Optional<std::string>
	 */

	std::expected<void, PreprocessorLogger> verify(const Program& program);
}
