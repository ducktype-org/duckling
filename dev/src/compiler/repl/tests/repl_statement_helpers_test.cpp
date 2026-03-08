#include <driver/repl_utils/repl_statement_helpers.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/utility.hpp>
#include <helios/mangler/mangler.hpp>

#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

using namespace compiler;

class ReplStatementHelpersTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ReplStatementHelpersTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testClassifySingleExpression);
		TESTER_ADD_TEST(testClassifySingleInstruction);
		TESTER_ADD_TEST(testClassifySingleDefinition);
		TESTER_ADD_TEST(testClassifyRejectsNonSingleInput);
		TESTER_ADD_TEST(testBuildExpressionWrapper);
		TESTER_ADD_TEST(testBuildInstructionWrapper);
		TESTER_ADD_TEST(testBuildWrapperRejectsDefinition);
		TESTER_ADD_TEST(testBuildWrapperRejectsMissingPayload);
		TESTER_ADD_TEST(testMakeExecutableHOUTUnit);
		TESTER_ADD_TEST(testCreateChainedStatementModule);
		TESTER_ADD_TEST(testGetStatementModuleName);
		TESTER_ADD_TEST(testGetDefinitionHOUTUnit);
	}

private:
	static frontend::ModuleID createModule(std::string_view code) {
		return frontend::createModuleTreeFromContents(code, "test_pkg");
	}

	void testClassifySingleExpression() {
		auto module_id = createModule("1 + 2;");

		query::utils::withContextDo([&](query::Context& ctx) {
			auto result = repl::classifySingleStatement(ctx, module_id);
			assertTrue(result.has_value(), "Expression classification should succeed");
			ASSERT_EQUAL(result->kind, repl::StatementExecutionKind::Expression);
			assertTrue(result->expr_stmt.has_value(), "Expression payload should be present");
			assertTrue(!result->instruction_stmt.has_value(), "Instruction payload should be empty");
			assertTrue(!result->definition_stmt.has_value(), "Definition payload should be empty");
		});
	}

	void testClassifySingleInstruction() {
		auto module_id = createModule("while (1 == 1) {}");

		query::utils::withContextDo([&](query::Context& ctx) {
			auto result = repl::classifySingleStatement(ctx, module_id);
			assertTrue(result.has_value(), "Instruction classification should succeed");
			ASSERT_EQUAL(result->kind, repl::StatementExecutionKind::Instruction);
			assertTrue(
				result->instruction_stmt.has_value(), "Instruction payload should be present"
			);
			assertTrue(!result->expr_stmt.has_value(), "Expression payload should be empty");
			assertTrue(!result->definition_stmt.has_value(), "Definition payload should be empty");
		});
	}

	void testClassifySingleDefinition() {
		auto module_id = createModule("fun foo() = {}");

		query::utils::withContextDo([&](query::Context& ctx) {
			auto result = repl::classifySingleStatement(ctx, module_id);
			assertTrue(result.has_value(), "Definition classification should succeed");
			ASSERT_EQUAL(result->kind, repl::StatementExecutionKind::Definition);
			assertTrue(result->definition_stmt.has_value(), "Definition payload should be present");
			assertTrue(!result->expr_stmt.has_value(), "Expression payload should be empty");
			assertTrue(!result->instruction_stmt.has_value(), "Instruction payload should be empty");
		});
	}

	void testClassifyRejectsNonSingleInput() {
		auto module_id = createModule("1 + 2;\n3 + 4;");

		query::utils::withContextDo([&](query::Context& ctx) {
			auto result = repl::classifySingleStatement(ctx, module_id);
			assertTrue(!result.has_value(), "Expected error for non-single-statement input");
			assertTrue(
				result.error().find("Expected exactly one top-level statement") != std::string::npos,
				"Expected a useful error message"
			);
		});
	}

	void testBuildExpressionWrapper() {
		auto module_id = createModule("1 + 2;");

		query::utils::withContextDo([&](query::Context& ctx) {
			auto classified = repl::classifySingleStatement(ctx, module_id);
			assertTrue(classified.has_value(), "Classification should succeed");

			auto wrapped = repl::buildStatementWrapper(ctx, classified.value(), 101);
			assertTrue(wrapped.has_value(), "Expression wrapper build should succeed");
			assertTrue(
				wrapped->wrapper_func_name.find("__repl_expr_wrapper_101") != std::string::npos,
				"Wrapper function name should include expression counter"
			);
		});
	}

	void testBuildInstructionWrapper() {
		auto module_id = createModule("while (1 == 1) {}");

		query::utils::withContextDo([&](query::Context& ctx) {
			auto classified = repl::classifySingleStatement(ctx, module_id);
			assertTrue(classified.has_value(), "Classification should succeed");

			auto wrapped = repl::buildStatementWrapper(ctx, classified.value(), 202);
			assertTrue(wrapped.has_value(), "Instruction wrapper build should succeed");
			assertTrue(
				wrapped->wrapper_func_name.find("__repl_instr_wrapper_202") != std::string::npos,
				"Wrapper function name should include instruction counter"
			);
		});
	}

	void testBuildWrapperRejectsDefinition() {
		auto module_id = createModule("fun foo() = {}");

		query::utils::withContextDo([&](query::Context& ctx) {
			auto classified = repl::classifySingleStatement(ctx, module_id);
			assertTrue(classified.has_value(), "Classification should succeed");

			auto wrapped = repl::buildStatementWrapper(ctx, classified.value(), 11);
			assertTrue(!wrapped.has_value(), "Definition wrapper build should fail");
			assertTrue(
				wrapped.error().find("Definitions do not have executable wrappers")
					!= std::string::npos,
				"Expected definition-wrapper error"
			);
		});
	}

	void testBuildWrapperRejectsMissingPayload() {
		query::utils::withContextDo([&](query::Context& ctx) {
			auto missing_expr = repl::buildStatementWrapper(
				ctx,
				repl::SingleStatementInfo{
					.kind             = repl::StatementExecutionKind::Expression,
					.expr_stmt        = {},
					.instruction_stmt = {},
					.definition_stmt  = {},
				},
				1
			);
			assertTrue(!missing_expr.has_value(), "Missing expression payload should fail");
			assertTrue(
				missing_expr.error().find("Missing expression statement payload")
					!= std::string::npos,
				"Expected missing-expression error"
			);

			auto missing_instr = repl::buildStatementWrapper(
				ctx,
				repl::SingleStatementInfo{
					.kind             = repl::StatementExecutionKind::Instruction,
					.expr_stmt        = {},
					.instruction_stmt = {},
					.definition_stmt  = {},
				},
				2
			);
			assertTrue(!missing_instr.has_value(), "Missing instruction payload should fail");
			assertTrue(
				missing_instr.error().find("Missing instruction statement payload")
					!= std::string::npos,
				"Expected missing-instruction error"
			);
		});
	}

	void testMakeExecutableHOUTUnit() {
		auto module_id = createModule("1 + 2;");

		query::utils::withContextDo([&](query::Context& ctx) {
			auto classified = repl::classifySingleStatement(ctx, module_id);
			assertTrue(classified.has_value(), "Classification should succeed");

			auto wrapped = repl::buildStatementWrapper(ctx, classified.value(), 303);
			assertTrue(wrapped.has_value(), "Wrapper build should succeed");

			auto hout_unit = repl::makeExecutableHOUTUnit(wrapped->wrapper_function);
			ASSERT_EQUAL(1UL, hout_unit.functions.size());
			ASSERT_EQUAL(
				hout_unit.functions[0]->declaration->original_symbol,
				wrapped->wrapper_function.declaration->original_symbol
			);
		});
	}

	void testCreateChainedStatementModule() {
		auto first_ref = repl::createChainedStatementModule("1 + 2;", {}, 7, "repl_");
		assertTrue(first_ref->isReplModule(), "Chained module should be marked as a REPL module");
		ASSERT_EQUAL("repl_7", first_ref->getName().strView());
		assertTrue(
			!first_ref->getReplModuleParent().has_value(),
			"First chained module should not have REPL parent"
		);
		assertTrue(first_ref->hasMainSourceFile(), "Chained module should have main source file");

		auto second_ref
			= repl::createChainedStatementModule("3 + 4;", first_ref->getModuleID(), 8, "script_");
		assertTrue(second_ref->isReplModule(), "Second module should be marked as a REPL module");
		ASSERT_EQUAL("script_8", second_ref->getName().strView());
		assertTrue(
			second_ref->getReplModuleParent().has_value(),
			"Second chained module should have REPL parent"
		);
		ASSERT_EQUAL(second_ref->getReplModuleParent().value(), first_ref->getModuleID());
	}

	void testGetStatementModuleName() {
		auto module_id = createModule("fun foo() = {}");

		auto default_name = repl::getStatementModuleName(module_id);
		assertTrue(
			default_name.starts_with("repl_module_"),
			"Default statement module prefix should be repl_module_"
		);

		auto custom_name = repl::getStatementModuleName(module_id, "script_module_");
		assertTrue(
			custom_name.starts_with("script_module_"),
			"Custom statement module prefix should be used"
		);
		assertTrue(default_name != custom_name, "Different prefixes should produce different names");
	}

	void testGetDefinitionHOUTUnit() {
		auto module_id = createModule("fun foo() = {}");

		query::utils::withContextDo([&](query::Context& ctx) {
			const auto& hout = repl::getDefinitionHOUTUnit(ctx, module_id);
			assertTrue(
				!hout.functions.empty() || !hout.glob_data.empty(),
				"Definition HOUT should contain emitted elements"
			);
		});
	}

public:
	~ReplStatementHelpersTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/repl/tests/");
