#include <filesystem/file.hpp>
#include <iostream>
#include <lexer/lexer.hpp>
#include <pst_parser/parser.hpp>

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "usage: ./element_testing file_name\n";
		return 1;
	}
	pst::init();
	fs::FilePath file(argv[1]);

	auto         tokens = lexer::tokenizeFile(file);
	auto         pst    = pst::parse(std::move(tokens));

	if (pst.getErrorState().fail()) {
		pst.getErrorState().dumpLog(std::cerr);
		std::cerr << "\nThere are errors, aborting.\n";
		pst.dprint(std::cerr);
		std::cerr << "\n";
	} else {
		pst.dprint(std::cerr);
		std::cerr << "\nDone.\n";
	}
}
