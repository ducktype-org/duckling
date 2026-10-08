// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file lexer_test.cpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#include <filesystem/file.hpp>
#include <init/init.hpp>
#include <lexer/lexer_class.hpp>
#include <logger/logger.hpp>
#include <token_source/source.hpp>

#include <iostream>

using namespace fs;

int main(int argc, char** argv) {
	init::InitObject _;
	if (argc != 2) {
		std::cerr << "usage: ./lexer_testing file_name\n";
		return 1;
	}

	logger::enableDevCategory(logger::DevLogCategories::Lexer);

	File path(argv[1]);
	auto source = tokenizer::makeTokenSource(path);
	source->tokenize();
	if (source->getIntLogger()->bad()) source->getIntLogger()->dumpLog(true, std::cerr);
}
