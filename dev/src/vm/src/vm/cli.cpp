#include "cli.hpp"

#include <diagnostic_interactive/module_flags/module_flags.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <vm/api/api.hpp>
#include <vm/api/data/api_error.hpp>
#include <vm/api/data/process_info.hpp>

#include <iostream>

std::string convertError(const vm::api::ApiError& api_error) {
	variant_match(api_error) {
		variant_case(vm::api::LoadProgramError, load) { return load.why; }
	}
	return vm::api::errorToString(api_error);
}

int cli() {
	std::string filepath;
	std::cout << "Path to file: ";
	std::cin >> filepath;

	return cli(fs::File(filepath));
}

int cli(const fs::File& filepath, const std::vector<std::string>& args) {
	vm::PID pid{};
	dia_int::configureTerminalPrinterColors(true);

	std::expected<i64, std::string> result
		= vm::api::spawn()
	          .and_then([&](vm::api::ProcessInfo info) {
				  pid = info.pid;

				  return std::expected<void, vm::api::ApiError>{};
			  })
	          .and_then([&] { return vm::api::loadFiles(pid, { filepath }); })
	          .and_then([&] { return vm::api::attach(pid, std::cin, std::cout); })
	          .and_then([&] { return vm::api::run(pid, args); })
	          .and_then([&] { return vm::api::join(pid); })
	          .and_then([&] { return vm::api::getExitValue(pid); })
	          .transform([&](vm::api::ExitValue vm_values) {
				  CORE_ASSERT(vm_values.size() == 1, "Program returned more than one return value");
				  auto& vm_value = vm_values.at(0);
				  CORE_ASSERT(
					  vm_value->type->getName() == base::StrID("i64"),
					  "DVM program returned and exit value different than i64"
				  );
				  return vm_value->readBytes<i64>();
			  })
	          .transform_error(convertError);

	if (result.has_value())
		return base::safeIntConv<int>(result.value());
	else {
		std::cerr << result.error();
		return 1;
	}
}
