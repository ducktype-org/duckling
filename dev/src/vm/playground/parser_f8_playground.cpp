#include <base/variant.hpp>

#include <vm/api/vm.hpp>

#include <iostream>

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "usage: ./vm_benchmark file_name\n";
		return 1;
	}
	fs::File file(argv[1]);

	auto process_pid = vm::api::spawn().value().pid;

	auto loaded_file_response = vm::api::loadFiles(process_pid, { file });

	if (!loaded_file_response.has_value()) {
		auto error = loaded_file_response.error();

<<<<<<< HEAD
		if_vrnt_is(error, vm::api::CoreOperationError, core_error) {
			if_vrnt_is(core_error, vm::api::LoadProgramError, load_error) {
=======
		variant_match(error) {
			variant_case(vm::api::LoadProgramError, load_error) {
>>>>>>> origin/main
				std::cerr << "Load errors: \n" << load_error.why << "\n";
			}
		}

	} else {
		std::cerr << "Running...\n";

		// this is not failing for some strange reason:
		// error is lost somewhere on api-vcpu path
		vm::api::run(process_pid).value();

		[[maybe_unused]]
		auto join_result
			= vm::api::join(process_pid);
	}
}
