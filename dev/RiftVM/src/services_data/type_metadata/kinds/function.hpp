#pragma once

#include <vector>
#include "../definitions.hpp"

namespace vm::kind {
	struct Function {
		std::vector<TypeCRef> parameters;
		TypeCRef result;
	};
}