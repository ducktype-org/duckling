#pragma once

#include <frontend/pst_parser/stable_position.hpp>

#include <base/collections/optional.hpp>

namespace compiler::mir {
	struct InstructionMetadata {
		base::Optional<pst::StablePosition> position;
	};
}
