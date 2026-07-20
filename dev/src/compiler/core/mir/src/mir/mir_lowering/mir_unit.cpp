#include "mir_unit.hpp"

#include "mir_queries.hpp"

#include <base/extend_cpp/variant_match.hpp>

namespace compiler::mir {
	query::QResult<MIRUnit> lowerToMIRUnit(query::Context& ctx, CRef<helios::HOUTUnit> hout_unit) {
		MIRUnit unit_result;
		bool    is_failed = false;

		// Functions:
		for (const auto& hout_function: hout_unit->functions) {
			CRef<LowerToMIRFunctionResult> mir_function
				= ctx.query<mir::LowerToMIRFunction>({ hout_function });

			if (mir_function->hasFailed())
				is_failed = true;
			else
				unit_result.mir_functions.emplace_back(&mir_function->valueOrPanic());
		}

		for (const auto& hout_global: hout_unit->glob_data) {
			// Discard information-less globals.
			if (not hout_global->type.getType().carriesInformation(ctx)) continue;
			auto mir_data_result = ctx.query<LowerGlobalData>({ hout_global });
			if (mir_data_result->hasFailed())
				is_failed = true;
			else
				unit_result.mir_globals.emplace_back(&mir_data_result->valueOrPanic());
		}


		if (is_failed) return query::Failed();

		return unit_result;
	}
}
