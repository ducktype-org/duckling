#pragma once

#include "../definitions.hpp"

#include <vector>

namespace vm::kind {
	struct Function {
		std::vector<TypeCRef> parameters;
		TypeCRef              result;
	};
}
