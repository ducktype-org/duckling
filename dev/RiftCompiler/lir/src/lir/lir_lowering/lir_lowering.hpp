#pragma once

#include <mir/mir_structure/mir_structure.hpp>

namespace compiler::lir {
	struct KeyOf_LowerToLirFunction {
		mir::Function& function;

		[[nodiscard]]
		base::HashT customPerfectHash() const;

		bool operator==(const KeyOf_LowerToMirFunction& oth) const {
			return function == oth.function;
		}
	};
}