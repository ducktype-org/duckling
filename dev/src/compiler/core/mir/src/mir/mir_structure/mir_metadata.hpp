#pragma once

#include <base/collections/optional.hpp>

#include <diagnostic/stable_position.hpp>

namespace compiler::mir {
	struct InstructionMetadata {
		base::Optional<dia::StablePosition> position;
	};
}
