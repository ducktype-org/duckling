#include <hir/hir.hpp>
#include <pst_parser/parser.hpp>
#include <filesystem/file.hpp>
#include <lexer/lexer.hpp>
#include <typesystem/typesystem.hpp>

hir::SourceUnit prepare(std::string_view filename) {
	fs::FilePath file(filename);
	return { { file }, file };
}

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "usage: ./hir_testing file_name\n";
		return 1;
	}
	std::string file_name(argv[1]);

	lexer::init();
	pst::init();
	ts::init();
	exec::init();

	hir::HIR hir;

	hir.addUnit(prepare(file_name));
	hir.doMagicStuff();
}
