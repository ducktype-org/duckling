#include <filesystem/file.hpp>
#include <lexer/lexer.hpp>
#include <pst_parser/parser.hpp>
#include <iostream>

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "usage: ./lsptree_interface file_name\n";
		return 1;
	}
	pst::init();
	fs::FilePath file(argv[1]);

	auto tokens = lexer::tokenizeFile(file);
	auto pst    = pst::parse(std::move(tokens));

	if (pst.getErrorState().fail()) pst.getErrorState().dumpLog(std::cerr);
	pst.dprint(std::cout);
}
