#include "compile_dvm.hpp"

#include <backends/dvm/backend.hpp>

#include <base/int_conv.hpp>

#include <vm/api/data/api_error.hpp>
#include <vm/api/data/process_info.hpp>
#include <vm/api/vm.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/serializer/serializer.hpp>

#include <expected>

namespace compiler::driver {
	vm::code::CodeCollection compileLIRModuleToDVM(
		query::Context&                query_ctx,
		const LIRModuleData&       data
		// const artifacts::FileArtifact& output_artifact
	) {
		std::vector<backend_vm::BackendDVMGlobal> dvm_globals;
		for (const auto& global: data.globals) {
			backend_vm::BackendDVMGlobal dvm_global{
				.lir_global  = global.lir_global,
				.global_ctor = global.global_ctor,
				.global_dtor = global.global_dtor,
			};
			dvm_globals.emplace_back(std::move(dvm_global));
		}
		backend_vm::Module       module{ query_ctx, data.module_id, data.functions, dvm_globals };
	
		return module.build();
		// this->code_collection.emplace_back(std::move(code_collection));
	}

	// std::expected<RunOutput, std::string> DVMDriver::run() {
	// 	if (code_collection.size() == 0)
	// 		return std::unexpected<std::string>{ "No code collection available." };

	// 	vm::PID pid{};

	// 	return vm::api::spawn()
	// 	    .and_then([&](vm::api::ProcessInfo process) {
	// 			pid = process.pid;

	// 			if (options->add_builtin_library) return vm::api::loadStdlib(pid);
	// 			return std::expected<void, vm::api::ApiError>{};
	// 		})
	// 	    .and_then([&] { return vm::api::loadCode(pid, { code_collection }); })
	// 	    .and_then([&] { return vm::api::attach(pid, std::cin, std::cout); })
	// 	    .and_then([&] { return vm::api::run(pid); })
	// 	    .and_then([&] { return vm::api::join(pid); })
	// 	    .and_then([&] { return vm::api::getExitCode(pid); })
	// 	    .transform_error(vm::api::errorToString)
	// 	    .transform([](auto exit_code) {
	// 			return RunOutput{ .exit_code = base::safeIntConv<int>(exit_code) };
	// 		});
	// }
}
