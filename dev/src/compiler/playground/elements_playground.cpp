// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <frontend/pst_parser/parsed_pst.hpp>

#include <filesystem/file.hpp>
#include <init/init.hpp>

#include <iostream>

int main(int argc, char** argv) {
	init::InitObject _;

	if (argc != 2) {
		std::cerr << "usage: ./element_testing file_name\n";
		return 1;
	}
	fs::File file(argv[1]);
	auto     pst = pst::ParsedPST<>::fromFile(file, pst::PSTType::Program);

	if (pst->getLogger()->bad()) {
		pst->getLogger()->dumpLog(false, std::cerr);
		std::cerr << "\nThere are errors, aborting.\n";
		pst->dprint(std::cerr);
		std::cerr << "\n";
	} else {
		pst->dprint(std::cerr);
		std::cerr << "\nDone.\n";
	}
}
