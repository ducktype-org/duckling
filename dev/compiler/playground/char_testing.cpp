#include <filesystem/encoding.hpp>
#include <lexer/decode.hpp>
#include <filesystem/file.hpp>
#include <diagnostic/logger.hpp>
#include <token_file/file.hpp>

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "usage: ./char_testing file_name\n";
		return 1;
	}
	fs::FilePath path(argv[1]);
	auto         file = tokenizer::makeTokenFile(path);

	file->decode<fs::Encoding::UTF8>();

	if (file->getLogger().bad()) {
		file->getLogger().dumpLog(true, std::cerr);
		return 0;
	}

	for (const auto& c: file->getChars()) std::cout << c.rawStr() << " ";
	std::cout << "\n";
}
