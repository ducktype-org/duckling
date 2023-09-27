#include <filesystem/file.hpp>
#include <iostream>
#include <lexer/char.hpp>

int main(int argc, char **argv) {
	if (argc != 2) {
		std::cerr << "usage: ./char_testing file_name\n";
		return 1;
	}
	fs::FilePath file(argv[1]);
	auto         file_content = file.getContent();

	auto chars = lexer::decode<fs::UTF8>(file_content.view());

	for (const auto &c : chars.getArray()) std::cout << c.rawStr() << " ";
	std::cout << "\n";
}
