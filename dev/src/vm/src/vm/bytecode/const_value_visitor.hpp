#pragma once

#include "const_value.hpp"

#include <base/extend_cpp/visitor.hpp>

namespace vm::code {
	MAKE_VISITOR(Const, ConstantImmediate, ConstantClass, ConstantFixedSizeTable);
}
