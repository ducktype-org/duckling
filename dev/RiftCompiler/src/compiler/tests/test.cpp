#include <compiler/compilation_handler.hpp>
#include <lexer/lexer.hpp>
#include <pst_parser/parser.hpp>
#include <tester/tester.hpp>

class CompilerTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS CompilerTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Compiler test") {
		lexer::init();
		pst::init();

		TESTER_ADD_TEST(testCompilerHandler);
	}

private:
	void testCompilerHandler() {
		compiler::CompilationHandler comp_handler;
		comp_handler.addFileRecursively(fs::FilePath(path("snippets/imports/main.rift")));

		assert(comp_handler.pstCount() == 7, "Wrong number of parsed files");
	}
};

TESTER_COMMON_MAIN("/RiftCompiler/src/compiler/tests/");
