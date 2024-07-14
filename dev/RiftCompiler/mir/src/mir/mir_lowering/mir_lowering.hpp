#pragma once

// @todo: not here?

#include <query_framework/query_int.hpp>
#include <helios/hout/hout.hpp>

#include "../mir_structure/mir_structure.hpp"

namespace compiler::mir {

	

	struct KeyOf_LowerToMirFunction {
		helios::HOUTFunction function;
		
		[[nodiscard]]
		base::HashT customPerfectHash() const;
		bool operator==(const KeyOf_LowerToMirFunction&) const = default;
	};

	DECLARE_QUERY(LowerToMirFunction, KeyOf_LowerToMirFunction, Function)
}
