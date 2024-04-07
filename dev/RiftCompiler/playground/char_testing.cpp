#include <lexer/decode.hpp>
#include <filesystem/file.hpp>
#include <diagnostic/logger.hpp>

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "usage: ./char_testing file_name\n";
		return 1;
	}
	fs::FilePath file(argv[1]);
	auto         file_content = file.getContent();

	dia::Logger logger;
	auto            chars = lexer::decode<fs::UTF8>(file_content.view(), logger);
	if (logger.bad()) {
		logger.dumpLog(true, std::cerr);
		return 0;
	}

	for (const auto& c: chars) std::cout << c.rawStr() << " ";
	std::cout << "\n";
}
