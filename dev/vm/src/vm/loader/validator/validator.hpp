#pragma once

#include <base/optional.hpp>

#include <vm/code/code.hpp>
#include <vm/loader/loader.hpp>
#include <vm/loader/parser/elements.hpp>

namespace vm::validator {
	/**
	 * @brief Validates the program.
	 * Returns an void in case of success and an error string on failure.
	 *
	 */
	std::expected<void, LoaderLogger> verify(const Program& program);
}
