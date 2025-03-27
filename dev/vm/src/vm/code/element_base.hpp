#pragma once

#include <base/optional.hpp>
#include <diagnostic/source_position.hpp>

namespace vm::code {
	struct ElementBase {
		base::Optional<dia::SourcePosition> bytecode_pos = {};

		bool operator==(const ElementBase& other) const = default;
	};
}
