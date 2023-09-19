#include "cli.hpp"

#include <api/api.hpp>
#include <json/json.hpp>
#include <supervisor/supervisor.hpp>

std::string convertError(const vm::api::ApiError& apiError) {
	if (std::holds_alternative<vm::api::WrongResponse>(apiError)) return "Wrong response";
	return std::visit(
		[](const auto&) {
			return "Error, json does not work\n";
			// return JS::serializeStruct(v); @TODO: issue #72
		},
		apiError);
}

template<class T, class E>
T expect(cpp::result<T, E> r) {
	cpp::result<T, std::string> r1 = r.map_error(convertError);
	if (r1.has_error()) {
		std::cout << r1.error() << "\n";
		std::exit(-1);
	}
	return r.value();
}

template<class E>
void expect(cpp::result<void, E> r) {
	cpp::result<void, std::string> r1 = r.map_error(convertError);
	if (r1.has_error()) {
		std::cout << r1.error() << "\n";
		std::exit(-1);
	}
}

void cli(std::string filepath) {
	vm::PID pid = expect(vm::api::spawn(true)).pid;
	expect(vm::api::loadFile(pid, fs::FilePath(filepath)));
	expect(vm::api::run(pid));
	expect(vm::api::join(pid));
}

void cli() {
	std::string filepath;
	std::cout << "Path to file: ";
	std::cin >> filepath;
	cli(filepath);
}
