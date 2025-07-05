#include "package_compilation_driver.hpp"

#include "backend_driver/llvm_ir_lib.hpp"

#include <backends/llvm/llvm_backend.hpp>
#include <driver/hout_to_binary_driver.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/queries.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/query_impl.hpp>

namespace compiler::driver {

	BackendOptions getBackendOptions(BackendType type) {
		if (type == BackendType::LLVM) {
			return BackendOptions{
				.backend_type         = type,
				.compile_to_assembly  = false,
				.dump_llvm_ir         = false,
				.dvm_code_only_memory = false,
				.add_builtin_library  = true,
			};
		} else if (type == BackendType::DVM) {
			return BackendOptions{
				.backend_type         = type,
				.compile_to_assembly  = false,
				.dump_llvm_ir         = false,
				.dvm_code_only_memory = false,
				.add_builtin_library  = true,
			};
		} else {
			CORE_UNREACHABLE();
		}
	}

	



	


}
