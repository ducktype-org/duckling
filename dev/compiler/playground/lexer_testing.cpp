/**
 * @file lexer_test.cpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#include <filesystem/file.hpp>
#include <lexer/lexer.hpp>
#include <lexer/lexer_class.hpp>
#include <token_file/file.hpp>
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

	FilePath path(argv[1]);
	auto     tokenFile = tokenizer::makeTokenFile(path);
	tokenFile->tokenize();
	if (tokenFile->getLogger().bad()) tokenFile->getLogger().dumpLog(true);
}
