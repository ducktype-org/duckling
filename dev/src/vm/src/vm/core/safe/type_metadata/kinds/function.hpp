#pragma once

#include "../definitions.hpp"

#include <vector>

namespace vm::kind {
	struct Function {
		std::vector<TypeCRef> parameters;
		std::vector<TypeCRef> result_types;
	};
}
