#pragma once

#include <diagnostic_interactive/hash_code_position.hpp>

#include <base/collections/optional.hpp>

namespace compiler::mir {
	struct InstructionMetadata {
		base::Optional<dia_int::HashCodePosition> position;
	};
}
