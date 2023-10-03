#include <filesystem/file.hpp>
#include <fstream>
#include <iostream>
#include <pst_parser/parser.hpp>
// #include <rift_parser/parsing_handler.hpp>
#include <lexer/lexer.hpp>
#include <sstream>
#include <tester/tester.hpp>
#include <tester/testing_utils.hpp>

class SimpleParserTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SimpleParserTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Simple Parser Test") {
		lexer::init();
		pst::init();

		TESTER_ADD_TEST(testIf);
		TESTER_ADD_TEST(testWhile);
		TESTER_ADD_TEST(testFun);
		TESTER_ADD_TEST(testFun2);
		TESTER_ADD_TEST(testBlock);
		TESTER_ADD_TEST(testActions);
		TESTER_ADD_TEST(testImport);
		TESTER_ADD_TEST(testUsing);
		TESTER_ADD_TEST(testNamespace);
		TESTER_ADD_TEST(testStruct);
		TESTER_ADD_TEST(testListParsing);
		TESTER_ADD_TEST(testListParsingErrors);
		TESTER_ADD_TEST(testUsingErrors);
		TESTER_ADD_TEST(testParamListErrors);
		TESTER_ADD_TEST(testMissingSemiErr);
		// TESTER_ADD_TEST(testParsingHandler);
	}

private:
	pst::PST prepare(const std::string& filename) {
		fs::FilePath file(filename);
		auto         td = lexer::tokenizeFile(file);
		return pst::parse(std::move(td));
	}

	void testJson(
		const std::string& rift_file, const std::string& json_file, bool no_errors = true
	) {
		pst::PST          pst = prepare(rift_file);
		std::stringstream ss;
		pst.dprint(ss);

		auto             correct_content = fs::getSimpleFileContent(json_file);
		std::string_view correct_string  = correct_content.view().stringView();

		if (no_errors)
			assert(pst.getErrorState().good(), "there are unexpected errors in rift source-code");

		assert(testing_utils::compareJson(ss.str(), correct_string), "outputs are not equal");
		// @TODO: Do we want to print some information about the differences or the
		// bad output to a file?
	}

	void testJsonRelativePath(
		const std::string& rift_file, const std::string& json_file, bool no_errors = true
	) {
		testJson(path("snippets/" + rift_file), path("snippets/" + json_file), no_errors);
	}

	void testIf() { testJsonRelativePath("if.rift", "if.json"); }

	void testWhile() { testJsonRelativePath("while.rift", "while.json"); }

	void testFun() { testJsonRelativePath("fun.rift", "fun.json"); }

	void testFun2() { testJsonRelativePath("fun2.rift", "fun2.json"); }

	void testBlock() { testJsonRelativePath("block.rift", "block.json"); }

	void testActions() { testJsonRelativePath("actions.rift", "actions.json"); }

	void testImport() { testJsonRelativePath("import.rift", "import.json"); }

	void testUsing() { testJsonRelativePath("using.rift", "using.json"); }

	void testNamespace() { testJsonRelativePath("namespace.rift", "namespace.json"); }

	void testStruct() { testJsonRelativePath("struct.rift", "struct.json"); }

	void testListParsing() { testJsonRelativePath("lists_ok.rift", "lists_ok.json"); }

	void testListParsingErrors() {
		pst::PST pst = prepare(path("snippets/lists_err.rift"));
		assert(pst.getErrorState().errCount() == 4, "Expected 4 errors");
	}

	void testUsingErrors() {
		pst::PST pst = prepare(path("snippets/using_err.rift"));
		assert(pst.getErrorState().errCount() == 3, "Expected 3 errors");
	}

	void testParamListErrors() {
		pst::PST pst = prepare(path("snippets/params_err.rift"));
		assert(pst.getErrorState().errCount() == 3, "Expected 3 errors");
	}

	void testMissingSemiErr() {
		pst::PST pst = prepare(path("snippets/missing_semicolon_err.rift"));
		assert(pst.getErrorState().errCount() == 4, "Expected 4 errors");
	}

public:
	~SimpleParserTest() override = default;
};

TESTER_COMMON_MAIN("/RiftCompiler/src/pst_parser/tests/");
