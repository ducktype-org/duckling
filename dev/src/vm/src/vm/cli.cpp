#include "cli.hpp"

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/defer.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <diagnostic/module_flags/module_flags.hpp>

#include <vm/api/api.hpp>
#include <vm/api/data/api_error.hpp>
#include <vm/api/data/process_info.hpp>
#include <vm/api/data/process_options.hpp>
#include <vm/bytecode/bytecode.hpp>

#include <iostream>

std::string convertError(const vm::api::ApiError& api_error) {
	variant_match(api_error) {
		variant_case(vm::api::LoadProgramError, load) { return load.why; }
	}
	return vm::api::errorToString(api_error);
}

int cli(
	const std::vector<fs::File>&    files,
	const std::vector<std::string>& args,
	const vm::api::ProcessConfig&   options,
	const std::vector<std::string>& ffi_libs
) {
	dia::configureTerminalPrinterColors(true);

	std::expected<i64, std::string> result
		= vm::api::spawn(options)
	          .and_then([&](vm::api::ProcessInfo info) -> std::expected<i64, vm::api::ApiError> {
				  const vm::PID pid = info.pid;
				  // Deinitialize the process and execute global destructors.
				  defer({
					  auto deinit = vm::api::deinitAndValidate(pid);
					  if (!deinit.has_value()) std::cerr << convertError(deinit.error()) << '\n';
				  });

				  return std::expected<void, vm::api::ApiError>{}
		              .and_then([&] -> std::expected<void, vm::api::ApiError> {
						  if (ffi_libs.empty()) return {};
						  // Registered before the bytecode loads, so `ffi function` declarations
			              // can resolve their symbols from these libraries.
						  vm::code::CodeCollection libs;
						  for (const auto& lib: ffi_libs) libs.object_files.emplace_back(lib);
						  return vm::api::loadCode(pid, libs);
					  })
		              .and_then([&] { return vm::api::loadFiles(pid, files); })
		              .and_then([&] { return vm::api::attach(pid, std::cin, std::cout); })
		              .and_then([&] { return vm::api::runAwait(pid, args); })
		              .transform([](vm::api::ExitValue vm_values) {
						  variant_match(vm_values) {
							  variant_case(i64, exit_code) { return exit_code; }
							  variant_case(std::vector<Ref<vm::IVMValue>>, values) {
								  CORE_ASSERT(
									  values.size() == 1,
									  "Program returned more than one return value"
								  );
								  auto& vm_value = values.at(0);
								  CORE_ASSERT(
									  vm_value->getType()->getName() == base::StrID("i64"),
									  "DVM program returned and exit value different than i64"
								  );
								  return vm_value->readBytes<i64>();
							  }
						  }
						  CORE_UNREACHABLE();
					  });
			  })
	          .transform_error(convertError);

	if (result.has_value())
		return base::safeIntConv<int>(result.value());
	else {
		std::cerr << result.error();
		return 1;
	}
}
