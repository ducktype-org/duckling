#include "cli.hpp"

#include <iostream>
#include <variant>

#include <base/variant.hpp>

#include <vm/api/api.hpp>
#include <vm/api/data/core_operation_error.hpp>
#include <vm/api/data/load_program_error.hpp>

#include "api/data/api_error.hpp"
#include <json/json.hpp>

std::string convertError(const vm::api::ApiError& api_error) {
	variant_match(api_error) {
		variant_case(vm::api::CoreOperationError, core) {
			variant_match(core) {
				variant_case(vm::api::LoadProgramError, load) { return load.why; }
			}
		}
		variant_case(vm::api::WrongResponse, _) return "Wrong response.";
	}
	return nlohmann::to_string(nlohmann::json(api_error));
}

template<class T, class E>
T expect(cpp::result<T, E> r) {
	cpp::result<T, std::string> r1 = r.map_error(convertError);
	if (r1.has_error()) {
		std::cout << r1.error() << "\n";
		std::exit(-1);  // NOLINT: Potential exit race condition
	}
	return r.value();
}

template<class E>
void expect(cpp::result<void, E> r) {
	cpp::result<void, std::string> r1 = r.map_error(convertError);
	if (r1.has_error()) {
		std::cout << r1.error() << "\n";
		std::exit(-1);  // NOLINT: Potential exit race condition
	}
}

void cli() {
	std::string filepath;
	std::cout << "Path to file: ";
	std::cin >> filepath;
	cli(fs::FilePath(filepath));
}

void cli(const fs::FilePath& filepath) {
	vm::PID pid = expect(vm::api::spawn()).pid;
	expect(vm::api::loadFile(pid, filepath));
	expect(vm::api::attach(pid, std::cin, std::cout));
	expect(vm::api::run(pid));
	expect(vm::api::join(pid));
}
