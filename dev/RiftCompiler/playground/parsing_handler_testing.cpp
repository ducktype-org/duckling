#include <filesystem/file.hpp>
#include <pst_parser/parser.hpp>
#include <compiler/compilation_handler.hpp>
#include <iostream>

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "usage: ./element_testing file_name\n";
		return 1;
	}
	pst::init();
	compiler::CompilationHandler comp_handler;
	fs::FilePath base_file_path = std::filesystem::path(argv[1]);

	comp_handler.addFileRecursively(base_file_path, true, &std::cerr);
}
