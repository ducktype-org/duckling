#pragma once

#include <diagnostic/source_position.hpp>

#include <base/optional.hpp>

namespace vm::code {
	struct ElementBase {
		base::Optional<dia::SourcePosition> bytecode_pos = {};

		// protected:
		bool operator==(const ElementBase& other) const = default;
	};
}
