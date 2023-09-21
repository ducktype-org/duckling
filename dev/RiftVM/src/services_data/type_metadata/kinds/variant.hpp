#pragma once

#include "../definitions.hpp"
#include <vector>

namespace vm::kind {
	struct Variant {
		std::vector<TypeRef> alternatives;
	};
}
