#include "dvm_driver.hpp"

#include <backends/dvm/backend.hpp>

#include "base/int_conv.hpp"

#include "vm/api/data/process_info.hpp"
#include "vm/bytecode/bytecode.hpp"
#include <vm/api/vm.hpp>
#include <vm/bytecode/serializer/serializer.hpp>

#include <fstream>

namespace compiler::driver {
	void DVMDriver::compileModule(query::Context& ctx, const BackendModuleData& data) {
		backend_vm::Module       module{ ctx, data.module_id, data.functions };
		vm::code::CodeCollection code_collection = module.build();
		std::ofstream            dvm_file(
            base::strConcat(data.module_id.strView(), ".dbc").c_str(), std::ios::binary
        );
		if (!dvm_file.is_open()) CORE_PANIC("Failed to open DVM file for writing");
		vm::code::serialize(code_collection, dvm_file);

		this->code_collection = code_collection;
		dvm_file.close();
	}

	void DVMDriver::link() {
		// DVM doesn't require linking.
	}

	std::expected<RunOutput, std::string> DVMDriver::run() {
		if (!code_collection.has_value())
			return std::unexpected<std::string>{ "No code collection available." };

		auto&   code = code_collection.value();
		vm::PID pid{};

		return vm::api::spawn()
		    .and_then([&](vm::api::ProcessInfo process) {
				pid = process.pid;
				return vm::api::loadCode(pid, { code });
			})
		    .and_then([&]() { return vm::api::run(pid); })
		    .and_then([&]() { return vm::api::join(pid); })
		    .and_then([&]() { return vm::api::getExitCode(pid); })
		    .transform_error([](const vm::api::ApiError& error) {
				return vm::api::errorToString(error);
			})
		    .transform([](auto exit_code) {
				return RunOutput{ .exit_code = base::safeIntConv<int>(exit_code) };
			});
	}
}
