#include "driver.hpp"
#include <base/exceptions.hpp>

namespace compiler::driver {

	Box<BackendDriver> createBackendStrategy(CRef<Options> options) {
		switch (options->backend_type) {
		case BackendType::LLVM:
			return base::makeBox<LLVMBackendDriver>(options);
		case BackendType::DuckBC:
			return base::makeBox<DuckBCBackendDriver>(options);
		default:
			CORE_PANIC("Wrong enum value");
		}
	}

	void LLVMBackendDriver::compile(BackendModuleData& lir_module) {
		backend_llvm::Module mod(lir_module.module_id);
		for (const auto& lir_function: lir_module.functions) mod.addFunctionToModule(lir_function);

		if (not mod.verify()) CORE_PANIC("LLVM module verification failed");

		std::cerr << "LLVM module compiled successfully\n";
		// This is the only way for not to output the module to the file
		mod.debugPrint();
	}

	void Driver::compileHOUTUnit(base::CRef<helios::HOUTUnit> hout_unit, base::StrID module_id) {
		std::vector<CRef<lir::Function>> functions;
		functions.reserve(hout_unit->functions.size());

		for (const auto& hout_function: hout_unit->functions) {
			auto mir_function = query::entryPoint<mir::LowerToMirFunction>({ hout_function });
			auto lir_function = query::entryPoint<lir::LowerToLirFunction>({ mir_function });
			functions.push_back(lir_function);
		}

		BackendModuleData module_data{ .module_id = module_id, .functions = functions };

		backend_driver->compile(module_data);
	}
}
