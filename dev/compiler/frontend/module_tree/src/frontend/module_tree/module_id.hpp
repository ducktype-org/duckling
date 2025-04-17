#pragma once

#include <base/strongly_typed_id.hpp>
#include <base/perfect_hash.hpp>

namespace compiler::frontend {
    STRONG_TYPEDEF_ID(ModuleID);

	// @TODO: move to STRONG_TYPEDEF_ID?
	inline base::HashT customPerfectHash(ModuleID id) { return id.asInt(); }
}
