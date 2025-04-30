#pragma once

#include <base/perfect_hash.hpp>
#include <base/strongly_typed_id.hpp>

namespace compiler::frontend {
	STRONG_TYPEDEF_ID(ModuleID);
	ID_PERFECT_HASH(ModuleID);
}
