#include <filesystem/file.hpp>
#include <iostream>
#include <lexer/lexer.hpp>
#include <pst_parser/pst.hpp>

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "usage: ./element_testing file_name\n";
		return 1;
	}
	pst::init();
	fs::FilePath file(argv[1]);
	pst::PST<>   pst(file);

	if (pst.getLogger().bad()) {
		pst.getLogger().dumpLog(false, std::cerr);
		std::cerr << "\nThere are errors, aborting.\n";
		pst.dprint(std::cerr);
		std::cerr << "\n";
	} else {
		pst.dprint(std::cerr);
		std::cerr << "\nDone.\n";
	}
}
