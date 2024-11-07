#pragma once

#include <mir/mir_structure/mir_structure.hpp>
#include "../lir_structure/lir_structure.hpp"

namespace compiler::lir {
	struct KeyOf_LowerToLirFunction {
		const mir::Function& function;

		[[nodiscard]]
		base::HashT customPerfectHash() const;

		bool operator==(const KeyOf_LowerToLirFunction& oth) const {
			// this kind of doesn't work, but it won't be run anyway (mir functions are unique)
			// @TODO: change it during hash-query refactor
			return function == oth.function;
		}
	};

	/**
	 * @brief Lower a MIRFunction to a LIRFunction
	 * Generates TSL types
	 */
	DECLARE_QUERY(LowerToLirFunction, KeyOf_LowerToLirFunction, const Function&);
}
