/**
 * @file compile_dvm.cpp
 * \parallel Must be thread-safe. Concurrent builds of the same module/package can collide on paths.
 */

#include "compile_dvm.hpp"

#include <backends/dvm/dvm_backend.hpp>
#include <time_stats/time_stats.hpp>

namespace compiler::driver {
	DVMModuleData compileLIRModuleToDVM(
		CRef<LIRModuleData> data, query::Context& query_ctx, bool build_debug_info
	) {
		time_stats::TrackCategoryTime _(time_stats::TimeCategories::BackendCompilation);

		backend_vm::DVMCodeBuilder module(query_ctx, build_debug_info);

		for (const auto& global: data->globals)
			module.insertLirGlobal(global.lir_global, global.global_ctor, global.global_dtor);

		for (const auto& lir_function: data->functions) {
			lir_function->debugPrint(query_ctx, std::cout);
			std::cout << '\n';
			module.insertLirFunction(lir_function);
		}

		auto code_collection = module.build();

		base::Optional<debug_info::DebugInfo> debug_info_opt;
		if (build_debug_info) debug_info_opt = module.buildDebugInfo();

		return {
			.code       = std::move(code_collection),
			.debug_info = std::move(debug_info_opt),
		};
	}
}
