#pragma once

#include "vm/preprocessor/preprocessor.hpp"
#include "vm/program/program.hpp"
#include <base/optional.hpp>
#include <vm/preprocessor/parser/elements.hpp>

namespace vm::validator {
	/**
	 * @brief Validates the program.
	 * Returns an empty optional in case of success and an error string on failure.
	 *
	 * @return base::Optional<std::string>
	 */

	std::expected<void, dia::Logger>
		verify(const program::Program& program, base::Optional<const PosMap&> pos_map);
}
