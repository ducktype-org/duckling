#pragma once

#include <base/optional.hpp>
#include <diagnostic/source_position.hpp>

namespace vm::program {
	struct ElementBase {
		base::Optional<dia::SourcePosition> bytecode_pos = {};
	};
}
