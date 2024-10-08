#pragma once

#include <query_framework/query_int.hpp>
#include <helios/hout/hout.hpp>

#include "../mir_structure/mir_structure.hpp"

namespace compiler::mir {

	struct KeyOf_LowerToMirFunction {
		helios::HOUTFunction& function;

		[[nodiscard]]
		base::HashT customPerfectHash() const;

		bool operator==(const KeyOf_LowerToMirFunction& oth) const {
			return function == oth.function;
		}
	};

	/**
	 * @brief Lower a HOUTFunction to a MIRFunction
	 * Performs lifetime analysis and checks
	 */
	DECLARE_QUERY(LowerToMirFunction, KeyOf_LowerToMirFunction, const Function&)

	/**
	 * @brief Lower a HOUTFunction to a "Pre" MIRFunction
	 * Does not perform lifetime analysis and any checks.
	 * @note Exposed in the interface mostly for tests
	 */
	Function lowerToPreMirFunction(query::Context&, const helios::HOUTFunction& function);

}
