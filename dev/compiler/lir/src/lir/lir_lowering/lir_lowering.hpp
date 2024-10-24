#pragma once

#include <mir/mir_structure/mir_structure.hpp>
#include "../lir_structure/lir_structure.hpp"

namespace compiler::lir {
	struct KeyOf_LowerToLirFunction {
		const mir::Function& function;

		[[nodiscard]]
		base::HashT customPerfectHash() const;

		bool operator==(const KeyOf_LowerToLirFunction& oth) const {
			return function == oth.function;

			// this still does not work, but the compiler errors are helpful now
		}
	};

	/**
	 * @brief Lower a MIRFunction to a LIRFunction
	 * Generates TSL types
	 */
	DECLARE_QUERY(LowerToLirFunction, KeyOf_LowerToLirFunction, const Function&);
}
