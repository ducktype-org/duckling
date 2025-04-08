#include "filesystem/file.hpp"
#include "helios/scopes/scopes.hpp"
#include <lexer/lexer.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/queries.hpp>
#include <clap/clap.hpp>
#include <iostream>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/query_impl.hpp>  // For logger only, @TODO relax it #404
#include <base/defer.hpp>
#include <init/init.hpp>

void printContextErrors() {
	if (query::Context::logger.messageCount() > 0) {
		std::cerr << "Compilation errors logged in context: \n";
		query::Context::logger.dumpLog(true, std::cerr);
	}
}

int notMain() {
	init::InitObject _;

	fs::FilePath path_to_compile = fs::FilePath("/home/krzysiek/rift/duckling/dev/compiler/helios/tests/test_modules/simple_scopes");

	using namespace compiler;

	auto root = query::entryPoint<frontend::QueryModuleTree>(path_to_compile);

	defer(printContextErrors());

	std::cerr << "Fetching main module.\n";
	auto main_hout = query::entryPoint<helios::QueryModuleHOUT>(root);
	std::cerr << "Main module HOUT fetched:\n";
	std::cerr << main_hout.debugPrint();

	auto houts = query::entryPoint<compiler::helios::QueryModuleHOUTRecursively>(root);

	return 0;
}

int main() {
	// note: we need to catch exception here,
	// because otherwise stack unwinding might not happen,
	// and defers might not be called.
	try {
		return notMain();
	} catch (std::exception& e) { std::cerr << "exception was thrown: " << e.what() << '\n'; }
}
