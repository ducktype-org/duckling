#include <filesystem/file.hpp>
#include <fstream>
#include <iostream>
#include <pst_parser/pst.hpp>
#include <pst_parser/pst_visitor.hpp>


#include <lexer/lexer.hpp>
#include <sstream>
#include <tester/tester.hpp>
#include <tester/testing_utils.hpp>

#define PSTVISITOR_METHOD(name)                        \
	bool visited_##name = false;                       \
	void visit##name(const pst::name& stmt) override { \
		if (!visited_##name) {                         \
			visited_##name = true;                     \
			counter++;                                 \
		}                                              \
		std::cout << "Visited " << #name << '\n';      \
		T::visit##name(stmt);                          \
	}

template<class T>
requires std::is_base_of_v<pst::PstStmtVisitor, T>
class PstStmtVisitorTester final: public T {
public:
	int counter = 0;

	PSTVISITOR_METHOD(Attribute)
	PSTVISITOR_METHOD(Import)
	PSTVISITOR_METHOD(Using)
	PSTVISITOR_METHOD(Alias)
	PSTVISITOR_METHOD(Expr)
	PSTVISITOR_METHOD(Return)
	PSTVISITOR_METHOD(Redo)
	PSTVISITOR_METHOD(Break)
	PSTVISITOR_METHOD(Continue)
	PSTVISITOR_METHOD(Defer)
	PSTVISITOR_METHOD(Throw)
	PSTVISITOR_METHOD(Const)
	PSTVISITOR_METHOD(Block)
	PSTVISITOR_METHOD(Namespace)
	PSTVISITOR_METHOD(Struct)
	PSTVISITOR_METHOD(Fun)
	PSTVISITOR_METHOD(Variable)
	PSTVISITOR_METHOD(If)
	PSTVISITOR_METHOD(While)
};

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
		TESTER_ADD_TEST(testVisitor);
		TESTER_ADD_TEST(testVisitorAlternative);

		// TESTER_ADD_TEST(testParsingHandler)
	}

private:
	pst::PST<> prepare(const std::string& filename) { return { fs::FilePath(filename) }; }

	void testVisitorImpl(const std::string& filename, usize expected_counter) {
		auto pst            = prepare(path(filename));
		auto panicky_vistor = PstStmtVisitorTester<pst::PstStmtVisitorPanicky>();
		auto empty_vistor   = PstStmtVisitorTester<pst::PstStmtVisitorEmpty>();
		for (auto&& stmt: pst.getRootElement()->getStatements()) {
			assertThrows<base::Panic>(
				[&] { stmt->acceptVisitor(panicky_vistor); }, "Stmt did not call it\'s visitor"
			);
			stmt->acceptVisitor(empty_vistor);
		}
		ASSERT_EQUAL(expected_counter, panicky_vistor.counter);
		ASSERT_EQUAL(expected_counter, empty_vistor.counter);
	}

	void testVisitor() { testVisitorImpl("snippets/all_statements.txt", 19); }

	void testVisitorAlternative() { testVisitorImpl("snippets/alternative_statements.txt", 1); }

	void testJson(
		const std::string& rift_file, const std::string& json_file, bool no_errors = true
	) {
		pst::PST<>        pst = prepare(rift_file);
		std::stringstream ss;
		pst.dprint(ss);

		auto             correct_content = fs::getSimpleFileContent(json_file);
		std::string_view correct_string  = correct_content.view().stringView();

		if (no_errors) {
			// if (pst.getErrorState().fail()) pst.getErrorState().dumpLog();
			assert(pst.getLogger().good(), "there are unexpected errors in rift source-code");
		}

		assert(testing_utils::compareJson(ss.str(), correct_string), "outputs are not equal");
		// @TODO: Do we want to print some information about the differences or the bad output to a
		// file?
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
		pst::PST<> pst = prepare(path("snippets/lists_err.rift"));
		assert(
			pst.getLogger().messageCount(dia::Message::Severity::Error) == 5, "Expected 5 errors"
		);
	}

	void testUsingErrors() {
		pst::PST<> pst = prepare(path("snippets/using_err.rift"));
		assert(
			pst.getLogger().messageCount(dia::Message::Severity::Error) == 2, "Expected 2 errors"
		);
	}

	void testParamListErrors() {
		pst::PST<> pst = prepare(path("snippets/params_err.rift"));
		assert(
			pst.getLogger().messageCount(dia::Message::Severity::Error) == 7, "Expected 7 errors"
		);
	}

	void testMissingSemiErr() {
		pst::PST<> pst = prepare(path("snippets/missing_semicolon_err.rift"));
		assert(
			pst.getLogger().messageCount(dia::Message::Severity::Error) == 2, "Expected 2 errors"
		);
	}


public:
	~SimpleParserTest() override = default;
};

TESTER_COMMON_MAIN("/RiftCompiler/pst_parser/tests/");
