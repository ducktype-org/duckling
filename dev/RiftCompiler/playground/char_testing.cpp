#include <lexer/decode.hpp>
#include <filesystem/file.hpp>
#include <printer/printer.hpp>
#include <diagnostic/error_state.hpp>

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "usage: ./char_testing file_name\n";
		return 1;
	}
	fs::FilePath file(argv[1]);
	auto         file_content = file.getContent();

	dia::ErrorState errorState;
	auto chars = lexer::decode<fs::UTF8>(file_content.view(), errorState);
	if (errorState.fail()) {
		errorState.dumpLog(std::cerr);
		return 0;
	}

	for (const auto& c: chars) std::cout << c.rawStr() << " ";
	std::cout << "\n";
}
