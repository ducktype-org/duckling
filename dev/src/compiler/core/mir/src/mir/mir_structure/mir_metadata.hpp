#pragma once

#include <base/collections/optional.hpp>
#include <frontend/pst_parser/stable_position.hpp>

namespace compiler::mir {
	struct InstructionMetadata {
		base::Optional<pst::StablePosition> position;
	};
}