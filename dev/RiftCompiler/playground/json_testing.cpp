#include <filesystem/file.hpp>
#include <lexer/lexer.hpp>
#include <pst_parser/parser.hpp>
#include <iostream>

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "usage: ./element_testing file_name\n";
		return 1;
	}
	pst::init();
	fs::FilePath file(argv[1]);

	auto printJson = [](const dia::Logger& log) {
		log.dumpLog<dia::DiagnosticToJSONConverter>(false, std::cout);
	};

	auto token_file = lexer::tokenizeFile(file);

	if (token_file->getLogger().bad()) {
		printJson(token_file->getLogger());
	} else {
		auto pst = pst::parse(std::move(token_file));
		if (pst.getLogger().bad()) printJson(pst.getLogger());
	}
}
