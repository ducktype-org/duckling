#include <lexer/char.hpp>
#include <filesystem/file.hpp>
#include <iostream>
#include <printer/printer.hpp>

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "usage: ./char_testing file_name\n";
		return 1;
	}
	fs::FilePath file(argv[1]);
	auto         file_content = file.getContent();

	printer::Console console;
	auto res = lexer::decode<fs::UTF8>(file_content.view(), console);
	if (!res) {
		console.print(std::cerr);
		return 0;
	}
	auto chars = std::move(res.value());

	for (const auto& c: chars.getArray()) std::cout << c.rawStr() << " ";
	std::cout << "\n";
}
