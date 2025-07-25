
#include "backend_driver.hpp"

#include "dvm_driver.hpp"
#include "llvm_driver.hpp"

namespace compiler::driver {
	Box<BackendDriver> createBackendDriver(CRef<BackendOptions> options) {
		switch (options->backend_type) {
		case BackendType::LLVM:
			return base::makeBox<LLVMDriver>(options);
		case BackendType::DVM:
			return base::makeBox<DVMDriver>(options);
		default:
			CORE_PANIC("Wrong enum value");
		}
	}
}
