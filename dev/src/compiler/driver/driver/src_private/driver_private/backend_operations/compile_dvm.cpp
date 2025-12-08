#include "compile_dvm.hpp"

#include "../statistics_private/statistics.hpp"

#include <backends/dvm/dvm_backend.hpp>
#include <timer/timer.hpp>

namespace compiler::driver {
	vm::code::CodeCollection compileLIRModuleToDVM(const LIRModuleData& data) {
		timer::AddToTime _(&backend_compilation_time);

		backend_vm::Module module(data.module_id);

		for (const auto& global: data.globals)
			module.insertLirGlobal(global.lir_global, global.global_ctor, global.global_dtor);

		for (const auto& lir_function: data.functions) module.insertLirFunction(lir_function);

		return module.build();
	}
}
