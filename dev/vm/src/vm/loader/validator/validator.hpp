#pragma once

#include <base/optional.hpp>

#include <vm/code/code.hpp>
#include <vm/loader/parser/elements.hpp>
#include <vm/loader/loader.hpp>

namespace vm::validator {
	/**
	 * @brief Validates the program.
	 * Returns an empty optional in case of success and an error string on failure.
	 *
	 * @return base::Optional<std::string>
	 */

	std::expected<void, PreprocessorLogger> verify(const Program& program);
}
