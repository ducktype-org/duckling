#include <diagnostic_interactive/stable_position.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/all_lists.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/pst.hpp>
#include <frontend/pst_parser/pst_visitor.hpp>
#include <frontend/pst_parser/test_utils/pst_test_utils.hpp>

#include <filesystem/file.hpp>
#include <tester/tester.hpp>
#include <tester/testing_utils.hpp>

#include <iostream>
#include <sstream>

#define PSTVISITOR_METHOD(name)                              \
	bool visited_##name = false;                             \
	void visit##name(pst::Access<pst::name> stmt) override { \
		if (!visited_##name) {                               \
			visited_##name = true;                           \
			counter++;                                       \
		}                                                    \
		std::cout << "Visited " << #name << '\n';            \
		T::visit##name(stmt);                                \
	}

template<class T>
requires std::is_base_of_v<pst::PstVisitor, T> class PstVisitorTester final: public T {
public:
	int counter = 0;

	PSTVISITOR_METHOD(Import)
	PSTVISITOR_METHOD(Using)
	PSTVISITOR_METHOD(Alias)
	PSTVISITOR_METHOD(ExprStmt)
	PSTVISITOR_METHOD(Return)
	PSTVISITOR_METHOD(Redo)
	PSTVISITOR_METHOD(Break)
	PSTVISITOR_METHOD(Continue)
	PSTVISITOR_METHOD(Defer)
	PSTVISITOR_METHOD(Throw)
	PSTVISITOR_METHOD(Const)
	PSTVISITOR_METHOD(Block)
	PSTVISITOR_METHOD(Namespace)
	PSTVISITOR_METHOD(Class)
	PSTVISITOR_METHOD(Fun)
	PSTVISITOR_METHOD(Variable)
	PSTVISITOR_METHOD(If)
	PSTVISITOR_METHOD(While)
};

class SimpleParserTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SimpleParserTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testIf);
		TESTER_ADD_TEST(testWhile);
		TESTER_ADD_TEST(testFor);
		TESTER_ADD_TEST(testFun);
		TESTER_ADD_TEST(testFun2);
		TESTER_ADD_TEST(testBlock);
		TESTER_ADD_TEST(testActions);
		TESTER_ADD_TEST(testImport);
		TESTER_ADD_TEST(testUsing);
		TESTER_ADD_TEST(testNamespace);
		TESTER_ADD_TEST(testClass);
		TESTER_ADD_TEST(testListParsing);
		TESTER_ADD_TEST(testListParsingErrors);
		TESTER_ADD_TEST(testUsingErrors);
		TESTER_ADD_TEST(testParamListErrors);
		TESTER_ADD_TEST(testMissingSemiErr);
		TESTER_ADD_TEST(testVisitor);
		TESTER_ADD_TEST(testVisitorAlternative);
		TESTER_ADD_TEST(testFunctionParameterVisitors);
		TESTER_ADD_TEST(testFunDeclFFI);
		TESTER_ADD_TEST(testSimpleExpand);

		// TESTER_ADD_TEST(testParsingHandler)
	}

private:
	pst::PST<> prepare(const std::string& filename) {
		return { fs::File(filename), pst::PSTType::Program };
	}

	void testVisitorImpl(const std::string& filename, usize expected_counter) {
		auto pst = prepare(path(filename));
		{
			auto panicky_visitor = PstVisitorTester<pst::PstVisitorPanicky>();
			auto empty_visitor   = PstVisitorTester<pst::PstVisitorEmpty>();
			for (const auto& stmt_locked:
			     pst.getRootElement().illegalAccess().value()->getStatements()) {
				auto stmt = stmt_locked.illegalAccess().value();
				assertThrows<base::Panic>(
					[&] { stmt->acceptVisitor(panicky_visitor); }, "Stmt did not call it\'s visitor"
				);
				stmt->acceptVisitor(empty_visitor);
			}
			ASSERT_EQUAL(expected_counter, panicky_visitor.counter);
			ASSERT_EQUAL(expected_counter, empty_visitor.counter);
		}

		// check that is also works when called from LangElement:
		{
			auto panicky_visitor = PstVisitorTester<pst::PstVisitorPanicky>();
			auto empty_visitor   = PstVisitorTester<pst::PstVisitorEmpty>();
			for (const auto& stmt_locked:
			     pst.getRootElement().illegalAccess().value()->getStatements()) {
				auto stmt = stmt_locked.illegalAccess().value();
				assertThrows<base::Panic>(
					[&] { stmt->acceptVisitor(panicky_visitor); },
					"LangElement did not call it\'s visitor"
				);
				stmt->acceptVisitor(empty_visitor);
			}
			ASSERT_EQUAL(expected_counter, panicky_visitor.counter);
			ASSERT_EQUAL(expected_counter, empty_visitor.counter);
		}
	}

	void testVisitor() { testVisitorImpl("snippets/all_statements.txt", 18); }

	void testVisitorAlternative() { testVisitorImpl("snippets/alternative_statements.txt", 1); }

	void testJson(
		const std::string& duckling_file, const std::string& json_file, bool no_errors = true
	) {
		pst::PST<> pst = prepare(duckling_file);
		if (not pst.hasErrors()) {
			assertTrue(
				pst::checkUniqueComponentHashs(pst.getRootElement()).isOk(),
				"Element paths are not unique"
			);
			assertTrue(
				pst::checkUniqueHashes(pst.getRootElement()).isOk(), "Element paths are not unique"
			);
		}
		std::stringstream ss;
		pst.dprint(ss);

		auto             correct_content = fs::File(json_file).getContent();
		std::string_view correct_string  = correct_content.view().stringView();

		if (no_errors) {
			// if (pst.getErrorState().fail()) pst.getErrorState().dumpLog();
			assertTrue(
				pst.getLogger()->good(), "there are unexpected errors in Duckling source-code"
			);
		}

		assertTrue(testing_utils::compareJson(ss.str(), correct_string), "outputs are not equal");
		if (no_errors) {
			assertTrue(
				pst::testElementCloning(CRef{ &*pst.getRootElement().illegalAccess().value() })
					.isOk(),
				"Error during cloning"
			);
		}
	}

	void testJsonRelativePath(
		const std::string& duckling_file, const std::string& json_file, bool no_errors = true
	) {
		testJson(path("snippets/" + duckling_file), path("snippets/" + json_file), no_errors);
	}

	void testIf() { testJsonRelativePath("if.duck", "if.json"); }

	void testWhile() { testJsonRelativePath("while.duck", "while.json"); }

	void testFor() { testJsonRelativePath("for.duck", "for.json"); }

	void testFun() { testJsonRelativePath("fun.duck", "fun.json"); }

	void testPattern() { testJsonRelativePath("pattern.duck", "pattern.json"); }

	void testFun2() { testJsonRelativePath("fun2.duck", "fun2.json"); }

	void testBlock() { testJsonRelativePath("block.duck", "block.json"); }

	void testActions() { testJsonRelativePath("actions.duck", "actions.json"); }

	void testImport() { testJsonRelativePath("import.duck", "import.json"); }

	void testUsing() { testJsonRelativePath("using.duck", "using.json"); }

	void testNamespace() { testJsonRelativePath("namespace.duck", "namespace.json"); }

	void testClass() { testJsonRelativePath("class.duck", "class.json"); }

	void testListParsing() { testJsonRelativePath("lists_ok.duck", "lists_ok.json"); }

	void testFormatStrParsing() {
		testJsonRelativePath("format_strings.duck", "format_strings.json");
	}

	void testFunDeclFFI() { testJsonRelativePath("ffi.duck", "ffi.json"); }

	void testNumericLiteralParsing() {
		testJsonRelativePath("numeric_literals.duck", "numeric_literals.json");
	}

	void testListParsingErrors() {
		pst::PST<> pst = prepare(path("snippets/lists_err.duck"));
		assertTrue(pst.getLogger()->errorCount() == 3, "Expected 3 errors");
	}

	void testUsingErrors() {
		pst::PST<> pst = prepare(path("snippets/using_err.duck"));
		assertTrue(pst.getLogger()->errorCount() == 2, "Expected 2 errors");
	}

	void testParamListErrors() {
		pst::PST<> pst = prepare(path("snippets/params_err.duck"));
		assertTrue(pst.getLogger()->errorCount() == 7, "Expected 7 errors");
	}

	void testMissingSemiErr() {
		pst::PST<> pst = prepare(path("snippets/missing_semicolon_err.duck"));
		assertTrue(pst.getLogger()->errorCount() == 3, "Expected 3 errors");
	}

	void testFunctionParameterVisitors() {
		pst::PST<> pst = prepare(path("snippets/function_with_parameters.duck"));
		assertTrue(pst.getLogger()->messageCount() == 0, "Expected 0 errors");

		auto fun_opt = pst.getRootElement()
		                   .illegalAccess()
		                   .value()
		                   ->getStatements()[0]
		                   .illegalAccess()
		                   .value()
		                   .dynamicCast<pst::Fun>();
		ASSERT_HAS_VALUE(fun_opt);
		auto fun = fun_opt.value();

		auto params = fun->getParams().illegalAccess().value();
		ASSERT_EQUAL(params->size(), 4);

		struct PstParamVisitor: public pst::PstVisitorPanicky {
			usize       counter   = 0;
			bool        good_name = false;
			base::StrID expected_name;

			PstParamVisitor(base::StrID expected_name): expected_name(expected_name) {}

			void visitParam(pst::Access<pst::Param> param) override {
				counter++;
				good_name = param->getName().illegalAccess().value()->unwrap() == expected_name;
			}
		};

		std::array names = { "a", "b", "c", "d" };

		usize i = 0;
		for (auto param: *params) {
			PstParamVisitor visitor(base::StrID(names.at(i)));
			param.illegalAccess().value()->acceptVisitor(visitor);
			ASSERT_EQUAL(visitor.counter, 1);
			i++;
		}
	}

	void testSimpleExpand() {
		auto pos      = dia_int::StablePosition::fakePosition();
		auto contents = "var a: T = 5;";
		auto pst
			= pst::PST<>::fromExpand(pos, contents, pst::LangParserContext::programBaseContext());
		assertTrue(pst.getLogger()->messageCount() == 0, "Expected 0 errors");
	}


public:
	~SimpleParserTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/frontend/pst_parser/tests/");
