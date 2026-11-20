// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "const_value.hpp"

#include <base/extend_cpp/visitor.hpp>

namespace vm::code {
	MAKE_VISITOR(Const, ConstantImmediate, ConstantClass, ConstantFixedSizeTable);
}
