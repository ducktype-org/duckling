#include "dvm_driver.hpp"
#include <fstream>

#include <backends/dvm/backend.hpp>
#include <vm/api/vm.hpp>
#include "vm/bytecode/bytecode.hpp"
#include <vm/bytecode/serializer/serializer.hpp>

namespace compiler::driver {
	void DVMDriver::compileModule(query::Context& ctx, const BackendModuleData& data) {
		backend_vm::Module module{
			ctx,
			data.module_id,
			data.functions
		};
		vm::code::CodeCollection code_collection = module.build();
		std::ofstream dvm_file(
			base::strConcat(data.module_id.strView(), ".dbc").c_str(),
			std::ios::binary
		);
		if (!dvm_file.is_open()) {
			CORE_PANIC("Failed to open DVM file for writing");
		}
		vm::code::serialize(code_collection, dvm_file);

		this->code_collection = code_collection;
		dvm_file.close();
	}

	void DVMDriver::link() {
		// DVM doesn't require linking.
	}

	void DVMDriver::run() {
		if (!code_collection.has_value()) {
			CORE_PANIC("No code collection to run");
		}
		auto& code = code_collection.value();

		auto r1 = vm::api::spawn();
		if (!r1.has_value()) {
			std::cout << errorToString(r1.error()) << "\n";
			CORE_PANIC("Failed to spawn process");
		}
		auto pid = r1.value().pid;

		// if (options->add_builtin_library) {
		// 	auto r0 = vm::api::loadStdlib();
		// 	if (!r0.has_value()) {
		// 		std::cout << errorToString(r0.error()) << "\n";
		// 		CORE_PANIC("Failed to load standard library");
		// 	}
		// }

		auto r3 = vm::api::loadCode(pid, {code});
		if (!r3.has_value()) {
			std::cout << errorToString(r3.error()) << "\n";
			CORE_PANIC("Failed to load code");
		}
		auto r4 = vm::api::run(pid);
		if (!r4.has_value()) {
			std::cout << errorToString(r4.error()) << "\n";
			CORE_PANIC("Failed to run process");
		}
		auto r5 = vm::api::join(pid);
		if (!r5.has_value()) {
			std::cout << errorToString(r5.error()) << "\n";
			CORE_PANIC("Failed to join process");
		}
		auto r6 = vm::api::getExitCode(pid);
		if (!r6.has_value()) {
			std::cout << errorToString(r6.error()) << "\n";
			CORE_PANIC("Failed to get exit code");
		}
		std::cout << "Exit code: " << r6.value() << "\n";
    }
}
