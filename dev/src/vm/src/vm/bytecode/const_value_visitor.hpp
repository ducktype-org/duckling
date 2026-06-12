#pragma once

#include "const_pool.hpp"

#include <base/extend_cpp/visitor.hpp>

namespace vm::code {
	MAKE_VISITOR(Const, ConstantImmediate, ConstantClass, ConstantFixedSizeTable);
}
