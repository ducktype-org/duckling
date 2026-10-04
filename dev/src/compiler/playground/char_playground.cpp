#include <diagnostic/logger.hpp>
#include <filesystem/encoding.hpp>
#include <filesystem/file.hpp>
#include <init/init.hpp>
#include <token_source/source.hpp>

int main(int argc, char** argv) {
	init::InitObject _;
	if (argc != 2) {
		std::cerr << "usage: ./char_testing file_name\n";
		return 1;
	}
	fs::File path(argv[1]);
	auto     file = tokenizer::makeTokenSource(path);

	file->decode<fs::Encoding::UTF8>();

	if (file->getIntLogger()->hasErrors()) return 0;

	for (const auto& c: file->getChars()) std::cout << c.rawStr() << " ";
	std::cout << "\n";
}
