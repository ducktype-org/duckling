#include <filesystem/file.hpp>
#include <init/init.hpp>
#include <iostream>
#include <lexer/lexer.hpp>
#include <pst_parser/pst.hpp>

int main(int argc, char** argv) {
	init::InitObject _;
	if (argc != 2) {
		std::cerr << "usage: ./json_testing file_name\n";
		return 1;
	}
	fs::FilePath file(argv[1]);

	pst::PST<> pst{ file };

	if (pst.getLogger().bad())
		pst.getLogger().dumpLog<dia::DiagnosticToJSONConverter>(false, std::cout);
}
