#include "hout_to_binary_driver.hpp"

#include <lir/lir_lowering/lir_lowering.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <mir/mir_lowering/mir_lowering.hpp>
// #include <query_framework/query_entry_point.hpp>
#include <query_framework/context.hpp>
#include <query_framework/utils/with_context_do.hpp>

#include <base/str_utils.hpp>
#include <base/string_id.hpp>

namespace compiler::driver {

	void HoutToBinaryDriver::compileHOUTUnit(
		query::Context&              ctx,
		base::CRef<helios::HOUTUnit> hout_unit,
		base::StrID                  module_id,
		artifacts::FileArtifact      output_artifact
	) {
		std::vector<CRef<lir::Function>> functions;
		functions.reserve(hout_unit->functions.size());

		for (const auto& hout_function: hout_unit->functions) {
			CRef mir_function = &ctx.query<mir::LowerToMirFunction>({ hout_function })->value();
			auto lir_function = ctx.query<lir::LowerToLirFunction>({ mir_function });
			functions.push_back(lir_function);
		}

		BackendModuleData module_data{ .module_id = module_id, .functions = functions };

		backend_driver->compileModule(ctx, module_data, output_artifact);
	}

	std::expected<RunOutput, std::string> HoutToBinaryDriver::run() {
		return backend_driver->run();
	}
}
