#pragma once

#include <base/collections/optional.hpp>

#include <diagnostic/source_position.hpp>

namespace vm::code {
	/**
	 * @brief This is a common base for all elements of high-level
	 * VM code representation.
	 */
	struct ElementBase {
		base::Optional<dia::SourcePosition> bytecode_pos = {};
	};
}
