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
			throw base::LogicError("Unknown backend type");
		}
	}

	void LLVMBackendDriver::compile(LIRModule& lir_module) {
		backend_llvm::Module mod("module_" + std::to_string(lir_module.module_id.asInt()));
		for (const auto& lir_function: lir_module.functions) mod.addFunctionToModule(lir_function);

		if (not mod.verify()) throw base::LogicError("LLVM module verification failed");

		std::cerr << "LLVM module compiled successfully\n";
		mod.debugPrint();
	}

	void Driver::compileHOUTUnit(
		base::CRef<helios::HOUTUnit> hout_unit, frontend::ModuleID module_id
	) {
		std::vector<CRef<lir::Function>> functions;
		functions.reserve(hout_unit->functions.size());

		for (const auto& hout_function: hout_unit->functions) {
			auto mir_function = query::entryPoint<mir::LowerToMirFunction>({ hout_function });
			auto lir_function = query::entryPoint<lir::LowerToLirFunction>({ mir_function });
			functions.push_back(lir_function);
		}

		LIRModule lir_module{ .module_id = module_id, .functions = functions };

		backend_driver->compile(lir_module);
	}
}
