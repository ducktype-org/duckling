#include <driver/repl_utils/repl_split_helpers.hpp>
#include <frontend/module_tree/module_tree.hpp>

#include <filesystem/file.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

class ExtractStatementSourcesTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ExtractStatementSourcesTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testEmptyInput);
		TESTER_ADD_TEST(testSingleStatement);
		TESTER_ADD_TEST(testMultipleStatements);
		TESTER_ADD_TEST(testSnippets);
	}

private:
	std::string readSnippet(const std::string& local_path) {
		return std::string{ fs::File(path(local_path)).getContent().view().stringView() };
	}

	std::vector<std::string> extractSources(std::string_view code) {
		auto module_id = compiler::frontend::createModuleTreeFromContents(code);

		std::vector<std::string> result;
		query::utils::withContextDo([&](query::Context& ctx) {
			result = compiler::repl::extractStatementSources(ctx, module_id);
		});
		return result;
	}

	void testEmptyInput() {
		auto result = extractSources("");
		assertTrue(result.empty(), "Expected no statements for empty input");
	}

	void testSingleStatement() {
		auto check_single = [&](std::string_view code) {
			auto result = extractSources(code);
			ASSERT_EQUAL(1UL, result.size());
			ASSERT_EQUAL(std::string(code), result[0]);
		};
		check_single("1 + 5;");
		check_single("var x: i32 = 5;");
		check_single("const X: i32 = 5;");
		check_single("fun foo() = {}");
		check_single("class Foo {}");
		check_single("namespace Foo {}");
	}

	void testMultipleStatements() {
		// two expression statements
		{
			auto result = extractSources("1 + 5;\n2 * 3;");
			ASSERT_EQUAL(2UL, result.size());
			ASSERT_EQUAL(std::string("1 + 5;"), result[0]);
			ASSERT_EQUAL(std::string("2 * 3;"), result[1]);
		}
		// mixed statement kinds, order preserved
		{
			auto result = extractSources("var x: i32 = 1;\n1 + 5;\nfun foo() = {}");
			ASSERT_EQUAL(3UL, result.size());
			ASSERT_EQUAL(std::string("var x: i32 = 1;"), result[0]);
			ASSERT_EQUAL(std::string("1 + 5;"), result[1]);
			ASSERT_EQUAL(std::string("fun foo() = {}"), result[2]);
		}
		// source order preserved for three calls
		{
			auto result = extractSources("foo();\nbar();\nbaz();");
			ASSERT_EQUAL(3UL, result.size());
			ASSERT_EQUAL(std::string("foo();"), result[0]);
			ASSERT_EQUAL(std::string("bar();"), result[1]);
			ASSERT_EQUAL(std::string("baz();"), result[2]);
		}
		// multi-line function body captured as a single statement
		{
			std::string code   = "fun foo() = {\n    var x: i32 = 1;\n}";
			auto        result = extractSources(code);
			ASSERT_EQUAL(1UL, result.size());
			ASSERT_EQUAL(code, result[0]);
		}
		// multiple declaration kinds
		{
			auto result = extractSources("class Foo {}\nnamespace Bar {}\nfun baz() = {}");
			ASSERT_EQUAL(3UL, result.size());
			ASSERT_EQUAL(std::string("class Foo {}"), result[0]);
			ASSERT_EQUAL(std::string("namespace Bar {}"), result[1]);
			ASSERT_EQUAL(std::string("fun baz() = {}"), result[2]);
		}
	}

	void testSnippets() {
		{
			auto result = extractSources(readSnippet("snippets/loops_and_declarations.duck"));
			ASSERT_EQUAL(4UL, result.size());
			ASSERT_EQUAL(std::string("var counter: i32 = 0;"), result[0]);
			ASSERT_EQUAL(std::string("class Storage {}"), result[3]);
			assertTrue(result[1].starts_with("while"), "Second statement must be the while loop");
			assertTrue(
				result[1].find("counter + 1") != std::string::npos,
				"While loop must contain the counter increment"
			);
			assertTrue(
				result[2].starts_with("fun doubleCounter"),
				"Third statement must be the function declaration"
			);
			assertTrue(
				result[2].find("while (i < n)") != std::string::npos,
				"Function body must contain nested while loop"
			);
		}
		{
			auto result = extractSources(readSnippet("snippets/declarations_only.duck"));
			ASSERT_EQUAL(6UL, result.size());
			ASSERT_EQUAL(std::string("import X as x;"), result[0]);
			ASSERT_EQUAL(std::string("using X;"), result[1]);
			ASSERT_EQUAL(std::string("using X as Y;"), result[2]);
			ASSERT_EQUAL(std::string("class Foo {}"), result[3]);
			ASSERT_EQUAL(std::string("namespace Math {}"), result[4]);
			assertTrue(
				result[5].starts_with("fun add"), "Last statement must be the function declaration"
			);
			assertTrue(
				result[5].find("return a + b;") != std::string::npos,
				"Function body must contain the return statement"
			);
		}
	}

public:
	~ExtractStatementSourcesTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/repl/tests/");
