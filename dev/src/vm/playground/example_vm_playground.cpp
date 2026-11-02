// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <clah/clah.hpp>

#include <vm/api/api.hpp>

auto runFile(const fs::File& file) -> void {
	auto process_pid_response = vm::api::spawn();
	if (!process_pid_response) CORE_PANIC("Spawn failed");
	auto pid = process_pid_response.value().pid;

	auto loaded_file_response = vm::api::loadFiles(pid, { file });
	if (!loaded_file_response) CORE_PANIC("Load failed");

	auto attach_response = vm::api::attach(pid, std::cin, std::cout);
	if (!attach_response) CORE_PANIC("Attach failed");

	auto run_response = vm::api::run(pid);
	if (!run_response) CORE_PANIC("Run failed");

	auto join_response = vm::api::join(pid);
	if (!join_response) CORE_PANIC("Join failed");
}

auto main(int argc, char** argv) -> int {
	init::InitObject _;

	auto clah = clah::Clah("example_vm_playground")
	                .addPositional(clah::FileParser::make("bytecode_file"))
	                .setHandler([](const clah::ParsingResult& result) -> int {
						auto file = result.getPositional<fs::File>(0);
						runFile(file);
						return 0;
					});

	return clah.execute(base::safeIntConv<usize>(argc), argv);
}
