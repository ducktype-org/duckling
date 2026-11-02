// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <filesystem/file.hpp>
#include <init/init.hpp>

#include <iostream>

using namespace fs;

int main(int argc, char** argv) {
	init::InitObject _;

	if (argc != 2) {
		std::cerr << "usage: ./file_testing file_name\n";
		return 1;
	}
	fs::File file(argv[1]);

	auto out = file.getContent();
	std::cout << out.view().size() << "\n";
	for (usize i = 0; i < out.view().size(); i++)
		std::cout << static_cast<unsigned>(out.view()[i]) << "\n";
}
