/**
 * @file lexer_test.cpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#include <filesystem/file.hpp>
#include <iostream>
#include <lexer/lexer.hpp>

using namespace fs;

int main(int argc, char **argv) {
	if (argc != 2) {
		std::cerr << "usage: ./lexer_testing file_name\n";
		return 1;
	}
	lexer::init();
	FilePath file(argv[1]);
	auto     tokens = lexer::tokenizeFile(file, true);
}
