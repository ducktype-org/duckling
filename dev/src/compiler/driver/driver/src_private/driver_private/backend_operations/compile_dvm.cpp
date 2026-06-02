/**
 * @file compile_dvm.cpp
 * \parallel Must be thread-safe. Concurrent builds of the same module/package can collide on paths.
 */

#include "compile_dvm.hpp"

#include <backends/dvm/dvm_backend.hpp>
#include <time_stats/time_stats.hpp>

#include <logger/logger.hpp>

namespace compiler::driver {
	static bool lirFunctionDealsWithStrings(const CRef<lir::Function> lir_function) {
		auto is_string_layout = [](const CRef<tsl::TypeLayout> layout) -> bool {
			return layout->getSourceType().getType().getKind() == tsh::Kind::String;
		};
		if (is_string_layout(lir_function->return_type_layout)) return true;
		for (const auto& param_layout: lir_function->parameter_layouts)
			if (is_string_layout(param_layout)) return true;
		return false;
	}

	DVMModuleData compileLIRModuleToDVM(
		CRef<LIRUnitWithBackendName> data, query::Context& query_ctx, bool build_debug_info
	) {
		time_stats::TrackCategoryTime _(time_stats::TimeCategories::BackendCompilation);

		backend_vm::DVMCodeBuilder module(query_ctx, build_debug_info, false);

		for (const auto& global: data->lir_unit.lir_globals) module.insertLirGlobal(global);

		for (const auto& lir_function: data->lir_unit.lir_functions) {
			// @TODO: #2483 Remove this filter (and the helper function) when strings work in DVM.
			if (lirFunctionDealsWithStrings(lir_function)) {
				CORE_DEV_LOG(
					Backend,
					"Lowering function that deals with strings skipped: ",
					lir_function->mangled_name,
					"\n"
				);
				continue;
			}
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
