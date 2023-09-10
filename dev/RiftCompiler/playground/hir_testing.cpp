#include <hir/hir.hpp>
#include <pst_parser/parser.hpp>
#include <filesystem/file.hpp>
#include <lexer/lexer.hpp>
#include <typesystem/typesystem.hpp>

hir::SourceUnit prepare(const fs::FilePath& file) {
	auto td = lexer::tokenizeFile(file.getContent());
	return {pst::parse(std::move(td)), file};
}

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "usage: ./hir_testing file_name\n";
		return 1;
	}
	fs::FilePath file(argv[1]);

	lexer::init();
	pst::init();
	ts::init();

	hir::HIR hir;
	
	hir.addUnit(prepare(file));
	hir.doMagicStuff();
}

