#pragma once

#include <diagnostic_interactive/stable_position.hpp>

#include <base/collections/optional.hpp>

namespace compiler::mir {
	struct InstructionMetadata {
		base::Optional<dia_int::StablePosition> position;
	};
}
