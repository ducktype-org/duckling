================================
High intermediate representation 
================================

.. attention::
 This is documentation of implementation of HIR. For general overview of HIR process see: :doc:`/source-doc/dev-handbook/compiler/hir`.

.. toctree::
	:caption: Code details:
	:titlesonly:
	:glob:

	symtable/index.rst
	*

Usage
=====

.. code-block:: cpp

    #include <hir/hir.hpp>
	#include <pst_parser/parser.hpp>
	#include <filesystem/file.hpp>
	#include <lexer/lexer.hpp>
	#include <typesystem/typesystem.hpp>

	hir::SourceUnit prepare(std::string_view filename) {
		fs::FilePath file(filename);
		auto         td = lexer::tokenizeFile(file);
		return { pst::parse(std::move(td)), file };
	}

	int main(int argc, char** argv) {
		// preparing file:
		if (argc != 2) {
			std::cerr << "usage: ./hir_testing file_name\n";
			return 1;
		}
		std::string file_name(argv[1]);

		lexer::init();
		pst::init();
		ts::init();
		exec::init();

		// running HIR:
		hir::HIR hir;
		hir.addUnit(prepare(file_name));
		hir.doMagicStuff();
	}
