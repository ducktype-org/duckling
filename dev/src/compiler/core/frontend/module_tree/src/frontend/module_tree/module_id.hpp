#pragma once

#include <base/strongly_typed_id.hpp>

#include <functional>  // IWYU pragma: export note: this is needed for std::hash

namespace compiler::frontend {
	STRONG_TYPEDEF_ID(ModuleID);
}

ID_STD_HASH(::compiler::frontend::ModuleID);
