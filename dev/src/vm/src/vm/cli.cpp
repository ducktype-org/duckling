#include "cli.hpp"

#include <base/variant.hpp>

#include <vm/api/api.hpp>
#include <vm/api/data/api_error.hpp>
#include <vm/api/data/core_operation_error.hpp>
#include <vm/api/data/load_program_error.hpp>
#include <vm/api/data/process_info.hpp>

#include <json/json.hpp>

#include <iostream>

std::string convertError(const vm::api::ApiError& api_error) {
	variant_match(api_error) {
		variant_case(vm::api::CoreOperationError, core) {
			variant_match(core) {
				variant_case(vm::api::LoadProgramError, load) { return load.why; }
			}
		}
	}
	return vm::api::errorToString(api_error);
}

int cli(bool load_stdlib) {
	std::string filepath;
	std::cout << "Path to file: ";
	std::cin >> filepath;

	return cli(fs::File(filepath), load_stdlib);
}

int cli(const fs::File& filepath, bool load_stdlib) {
	vm::PID pid{};

	std::expected<i64, std::string> result
		= vm::api::spawn()
	          .and_then([&](vm::api::ProcessInfo info) {
				  pid = info.pid;

				  if (load_stdlib) return vm::api::loadStdlib(pid);
				  return std::expected<void, vm::api::ApiError>{};
			  })
	          .and_then([&] { return vm::api::loadFiles(pid, { filepath }); })
	          .and_then([&] { return vm::api::attach(pid, std::cin, std::cout); })
	          .and_then([&] { return vm::api::run(pid); })
	          .and_then([&] { return vm::api::join(pid); })
	          .and_then([&] { return vm::api::getExitValue(pid); })
	          .transform([&](CRef<vm::VmValue> vm_value) { return vm_value->interpret<i64>(); })
	          .transform_error(convertError);

	if (result.has_value())
		return base::safeIntConv<int>(result.value());
	else {
		std::cerr << result.error();
		return 1;
	}
}
