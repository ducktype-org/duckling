#include "dvm_driver.hpp"

#include <backends/dvm/backend.hpp>

#include <base/int_conv.hpp>

#include <vm/api/data/api_error.hpp>
#include <vm/api/data/process_info.hpp>
#include <vm/api/vm.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/serializer/serializer.hpp>

#include <expected>
#include <fstream>

namespace compiler::driver {
	void DVMDriver::compileModule(query::Context&, const BackendModuleData& data) {
		backend_vm::Module       module{ data.module_id, data.functions };
		vm::code::CodeCollection code_collection = module.build();

		if (not options->dvm_code_only_memory) {
			std::ofstream dvm_file(
				base::strConcat(data.module_id.strView(), ".dbc").c_str(), std::ios::binary
			);
			if (!dvm_file.is_open()) CORE_PANIC("Failed to open DVM file for writing");
			vm::code::serialize(code_collection, dvm_file);
			dvm_file.close();
			std::cout << "DVM file written to: " << data.module_id.strView() << ".dbc\n";
		}

		this->code_collection.emplace_back(std::move(code_collection));
	}

	void DVMDriver::link() {
		// DVM doesn't require linking.
	}

	std::expected<RunOutput, std::string> DVMDriver::run() {
		if (code_collection.size() == 0)
			return std::unexpected<std::string>{ "No code collection available." };

		vm::PID pid{};

		return vm::api::spawn()
		    .and_then([&](vm::api::ProcessInfo process) {
				pid = process.pid;

				if (options->add_builtin_library) return vm::api::loadStdlib(pid);
				return std::expected<void, vm::api::ApiError>{};
			})
		    .and_then([&] { return vm::api::loadCode(pid, { code_collection }); })
		    .and_then([&] { return vm::api::attach(pid, std::cin, std::cout); })
		    .and_then([&] { return vm::api::run(pid); })
		    .and_then([&] { return vm::api::join(pid); })
		    .and_then([&] { return vm::api::getExitCode(pid); })
		    .transform_error(vm::api::errorToString)
		    .transform([](auto exit_code) {
				return RunOutput{ .exit_code = base::safeIntConv<int>(exit_code) };
			});
	}
}
