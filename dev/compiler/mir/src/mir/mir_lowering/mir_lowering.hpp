#pragma once

#include "../mir_structure/mir_structure.hpp"
#include <helios/hout/hout.hpp>
#include <query_framework/query_int.hpp>

namespace compiler::mir {

	struct KeyOf_LowerToMirFunction {
		// note that HOUTFunction copy is lightweight, cause its uses shared_ptr under the hood
		helios::HOUTFunction function;

		[[nodiscard]]
		base::HashT customPerfectHash() const;

		bool operator==(const KeyOf_LowerToMirFunction& oth) const {
			return function == oth.function;
		}
	};

	/**
	 * @brief Lower a HOUTFunction to a MIRFunction
	 * Performs lifetime analysis.
	 * @note in the future it will validate move semantics and potentially other things.
	 */
	DECLARE_QUERY(LowerToMirFunction, KeyOf_LowerToMirFunction, CRef<Function>)

	/**
	 * @brief Lower a HOUTFunction to a "Pre" MIRFunction.
	 * It creates MIR function, but does not perform lifetime analysis and or any checks.
	 * @note Exposed in the interface mostly for tests
	 */
	Function lowerToPreMirFunction(query::Context&, const helios::HOUTFunction& function);

}
