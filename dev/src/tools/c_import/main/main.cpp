/**
 * @file main.cpp
 * @brief `duck_c_import <request.json> [<report.json>]`: generates Duckling bindings for C
 * headers. Usually run through `duck translate-c`.
 */
#include <c_import/request.hpp>

#include <init/init.hpp>

#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char** argv) {
	init::InitObject _;
	if (argc < 2 || argc > 3) {
		std::cerr << "usage: duck_c_import <request.json> [<report.json>]\n";
		return 2;
	}

	std::ifstream in(argv[1]);
	if (!in) {
		std::cerr << "cannot read `" << argv[1] << "`\n";
		return 2;
	}
	std::stringstream text;
	text << in.rdbuf();

	auto request = c_import::parseRequest(text.str());
	if (!request) {
		std::cerr << request.error() << "\n";
		return 2;
	}
	if (request->read.resource_dir.empty()) request->read.resource_dir = DUCK_CLANG_RESOURCE_DIR;

	auto report = c_import::run(*request);
	auto json   = c_import::serializeReport(report);
	if (argc == 3)
		std::ofstream(argv[2]) << json;
	else
		std::cout << json;
	return report.errors.empty() ? 0 : 1;
}
