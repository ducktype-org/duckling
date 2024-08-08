#pragma once

#include <vector>
#include "../definitions.hpp"

namespace vm::kind {
	struct Variant {
		std::vector<TypeRef> alternatives;
	};
}
