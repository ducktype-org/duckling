/**
 * @file compile_dvm.cpp
 * \parallel Must be thread-safe. Concurrent builds of the same module/package can collide on paths.
 */

#include "compile_dvm.hpp"

#include <backends/dvm/dvm_backend.hpp>
#include <time_stats/time_stats.hpp>

namespace compiler::driver {
	vm::code::CodeCollection compileLIRModuleToDVM(CRef<LIRModuleData> data) {
		backend_vm::Module module(data->module_id);

		for (const auto& global: data->globals)
			module.insertLirGlobal(global.lir_global, global.global_ctor, global.global_dtor);

		for (const auto& lir_function: data->functions) module.insertLirFunction(lir_function);

		return module.build();
	}
}
