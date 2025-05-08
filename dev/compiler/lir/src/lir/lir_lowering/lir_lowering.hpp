#pragma once

#include "../lir_structure/lir_structure.hpp"

#include <mir/mir_structure/mir_structure.hpp>

namespace compiler::lir {
	struct KeyOf_LowerToLirFunction {
		CRef<mir::Function> function;

		[[nodiscard]]
		base::HashT customPerfectHash() const;

		bool operator==(const KeyOf_LowerToLirFunction& oth) const {
			// this kind of doesn't work, but it won't be run anyway (mir functions are unique)
			// @TODO: delete it during hash-query refactor #523
			return (*function) == (*oth.function);
		}
	};

	/**
	 * @brief Lower a MIRFunction to a LIRFunction
	 * Generates TSL types
	 */
	DECLARE_QUERY(LowerToLirFunction, KeyOf_LowerToLirFunction, CRef<Function>);
}
