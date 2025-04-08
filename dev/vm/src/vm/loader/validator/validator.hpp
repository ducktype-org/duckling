#pragma once

#include <vm/loader/loader.hpp>

namespace vm::loader::validator {
	/**
	 * @brief Validates the program.
	 * Returns an void in case of success and an error string on failure.
	 */
	std::expected<void, LoaderLogger> verify(const Program& program);
}
