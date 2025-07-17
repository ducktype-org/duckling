/**
 * @file lexer_test.cpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#include <lexer/lexer.hpp>
#include <lexer/lexer_class.hpp>
#include <token_source/source.hpp>

#include <filesystem/file.hpp>
#include <init/init.hpp>

#include <iostream>

using namespace fs;

int main(int argc, char** argv) {
	init::InitObject _;
	if (argc != 2) {
		std::cerr << "usage: ./lexer_testing file_name\n";
		return 1;
	}

	lexer::Lexer::setTokenMessages(true);

	File path(argv[1]);
	auto source = tokenizer::makeTokenSource(path);
	source->tokenize();
	if (source->getLogger()->bad()) source->getLogger()->dumpLog(true);
}
