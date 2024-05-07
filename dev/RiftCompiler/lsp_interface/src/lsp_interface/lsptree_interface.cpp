#include <filesystem/file.hpp>
#include <lexer/lexer.hpp>
#include <pst_parser/parser.hpp>
#include <iostream>

int main(int argc, char** argv) {
	if (argc < 2) {
		std::cerr << "usage: ./lsptree_interface file_content\n";
		return 1;
	}

	std::string file_content = argv[1];
	for (int i = 2; i < argc; i++) {
		file_content += " ";
		file_content += argv[i];
	}

	pst::init();
	const auto file = fs::FilePath::createTempFile(file_content);

	auto tokens = lexer::tokenizeFile(file);
	auto pst    = pst::parse(std::move(tokens));

	if (pst.getLogger().bad()) pst.getLogger().dumpLog(true);
	pst.getLSP(std::cout);
}
