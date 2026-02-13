/**
 * @file compile_dvm.cpp
 * \parallel Must be thread-safe. Concurrent builds of the same module/package can collide on paths.
 */

#include "compile_dvm.hpp"

#include <backends/dvm/dvm_backend.hpp>
#include <time_stats/time_stats.hpp>

#include <mutex>

namespace {
	/// DVM for now is not thread safe, so we need a mutex
	std::mutex dvm_compile_mutex;
}

namespace compiler::driver {
	vm::code::CodeCollection compileLIRModuleToDVM(CRef<LIRModuleData> data) {
		std::scoped_lock              lock(dvm_compile_mutex);
		time_stats::TrackCategoryTime _(time_stats::TimeCategories::BackendCompilation);

		backend_vm::Module module(data->module_id);

		for (const auto& global: data->globals)
			module.insertLirGlobal(global.lir_global, global.global_ctor, global.global_dtor);

		for (const auto& lir_function: data->functions) module.insertLirFunction(lir_function);

		return module.build();
	}
}
