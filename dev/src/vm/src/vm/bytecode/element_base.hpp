#pragma once

#include <base/collections/optional.hpp>

#include <diagnostic/source_position.hpp>

namespace vm::code {
	/**
	 * @brief This is a common base for all element of high-level
	 * VM code representation.
	 *
	 * @note In the future, there will be more fields including
	 * e.g. link to a high-level language element that this element
	 * corresponds to.
	 */
	struct ElementBase {
		base::Optional<dia::SourcePosition> bytecode_pos = {};
	};
}
