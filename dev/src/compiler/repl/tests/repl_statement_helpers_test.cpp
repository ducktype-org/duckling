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
		TESTER_ADD_TEST(testClassifyAssignmentExpressionAsInstruction);
		TESTER_ADD_TEST(testClassifySingleInstruction);
		TESTER_ADD_TEST(testClassifySingleDefinition);
		TESTER_ADD_TEST(testClassifySingleVariable);
		TESTER_ADD_TEST(testClassifyRejectsNonSingleInput);
		TESTER_ADD_TEST(testClassifyRejectsSingleActionStatements);
		TESTER_ADD_TEST(testBuildExpressionWrapper);
		TESTER_ADD_TEST(testBuildInstructionWrapper);
		TESTER_ADD_TEST(testBuildWrapperRejectsDefinition);
		TESTER_ADD_TEST(testBuildWrapperRejectsVariable);
		TESTER_ADD_TEST(testBuildVariableWrapper);
		TESTER_ADD_TEST(testMakeExecutableHOUTUnit);
		TESTER_ADD_TEST(testcreateSyntheticChainedStatementModule);
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
			ASSERT_HAS_VALUE(result, "Expression classification should succeed");
			ASSERT_MATCHES(*result, repl::ExpressionSingleStatementInfo);
		});
	}

	void testClassifyAssignmentExpressionAsInstruction() {
		auto assignment_test = [&](std::string_view code) {
			auto module_id = createModule(code);

			query::utils::withContextDo([&](query::Context& ctx) {
				auto result = repl::classifySingleStatement(ctx, module_id);
				ASSERT_HAS_VALUE(result, "Assignment classification should succeed");
				ASSERT_MATCHES(*result, repl::InstructionSingleStatementInfo);
			});
		};

		assignment_test("x = 10;");
		assignment_test("x += 10;");
		assignment_test("x -= 10;");
	}

	void testClassifySingleInstruction() {
		auto module_id = createModule("while (1 == 1) {}");

		query::utils::withContextDo([&](query::Context& ctx) {
			auto result = repl::classifySingleStatement(ctx, module_id);
			ASSERT_HAS_VALUE(result, "Instruction classification should succeed");
			ASSERT_MATCHES(*result, repl::InstructionSingleStatementInfo);
		});
	}

	void testClassifySingleDefinition() {
		auto module_id = createModule("fun foo() = {}");

		query::utils::withContextDo([&](query::Context& ctx) {
			auto result = repl::classifySingleStatement(ctx, module_id);
			ASSERT_HAS_VALUE(result, "Definition classification should succeed");
			ASSERT_MATCHES(*result, repl::DefinitionSingleStatementInfo);
		});
	}

	void testClassifySingleVariable() {
		auto module_id = createModule("var x = 1;");

		query::utils::withContextDo([&](query::Context& ctx) {
			auto result = repl::classifySingleStatement(ctx, module_id);
			ASSERT_HAS_VALUE(result, "Variable classification should succeed");
			ASSERT_MATCHES(*result, repl::VariableSingleStatementInfo);
		});
	}

	void testClassifyRejectsNonSingleInput() {
		auto module_id = createModule("1 + 2;\n3 + 4;");

		query::utils::withContextDo([&](query::Context& ctx) {
			auto result = repl::classifySingleStatement(ctx, module_id);
			ASSERT_NO_VALUE(result, "Expected error for non-single-statement input");
			assertTrue(
				result.error().find("Expected exactly one classified statement")
					!= std::string::npos,
				"Expected a useful error message"
			);
		});
	}

	void testClassifyRejectsSingleActionStatements() {
		auto assert_unclassified_action = [&](std::string_view code) {
			auto module_id = createModule(code);

			query::utils::withContextDo([&](query::Context& ctx) {
				auto result = repl::classifySingleStatement(ctx, module_id);
				ASSERT_NO_VALUE(result, "Expected action statement to be unclassified");
				assertTrue(
					result.error().find("Unsupported single statement kind for REPL classification")
						!= std::string::npos,
					"Expected explicit unsupported-kind error"
				);
				assertTrue(
					result.error().find("Action") != std::string::npos,
					"Expected unsupported kind to mention Action"
				);
			});
		};

		assert_unclassified_action("break;");
		assert_unclassified_action("continue;");
		assert_unclassified_action("throw 5;");
		assert_unclassified_action("return 2;");
	}

	void testBuildExpressionWrapper() {
		auto module_id = createModule("1 + 2;");

		query::utils::withContextDo([&](query::Context& ctx) {
			auto classified = repl::classifySingleStatement(ctx, module_id);
			ASSERT_HAS_VALUE(classified, "Classification should succeed");

			auto wrapped = repl::buildStatementWrapper(ctx, classified.value(), 101);
			ASSERT_HAS_VALUE(wrapped, "Expression wrapper build should succeed");
			assertTrue(
				wrapped->wrapper_func_name.find("__repl_input_wrapper_101") != std::string::npos,
				"Wrapper function name should include expression counter"
			);
		});
	}

	void testBuildInstructionWrapper() {
		auto module_id = createModule("while (1 == 1) {}");

		query::utils::withContextDo([&](query::Context& ctx) {
			auto classified = repl::classifySingleStatement(ctx, module_id);
			ASSERT_HAS_VALUE(classified, "Classification should succeed");

			auto wrapped = repl::buildStatementWrapper(ctx, classified.value(), 202);
			ASSERT_HAS_VALUE(wrapped, "Instruction wrapper build should succeed");
			assertTrue(
				wrapped->wrapper_func_name.find("__repl_input_wrapper_202") != std::string::npos,
				"Wrapper function name should include instruction counter"
			);
		});
	}

	void testBuildWrapperRejectsDefinition() {
		auto module_id = createModule("fun foo() = {}");

		query::utils::withContextDo([&](query::Context& ctx) {
			auto classified = repl::classifySingleStatement(ctx, module_id);
			ASSERT_HAS_VALUE(classified, "Classification should succeed");

			auto wrapped = repl::buildStatementWrapper(ctx, classified.value(), 11);
			ASSERT_NO_VALUE(wrapped, "Definition wrapper build should fail");
			assertTrue(
				wrapped.error().find("Definitions do not have executable wrappers")
					!= std::string::npos,
				"Expected definition-wrapper error"
			);
		});
	}

	void testBuildWrapperRejectsVariable() {
		auto module_id = createModule("var x = 1;");

		query::utils::withContextDo([&](query::Context& ctx) {
			auto classified = repl::classifySingleStatement(ctx, module_id);
			ASSERT_HAS_VALUE(classified, "Classification should succeed");

			auto wrapped = repl::buildStatementWrapper(ctx, classified.value(), 12);
			ASSERT_NO_VALUE(wrapped, "Variable wrapper build should fail");
			assertTrue(
				wrapped.error().find("do not have executable wrappers") != std::string::npos,
				"Expected non-executable-statement error"
			);
		});
	}

	void testBuildVariableWrapper() {
		auto module_id = createModule("var x = 1;");

		query::utils::withContextDo([&](query::Context& ctx) {
			auto classified = repl::classifySingleStatement(ctx, module_id);
			ASSERT_HAS_VALUE(classified, "Classification should succeed");

			auto variable_info
				= std::get<repl::VariableSingleStatementInfo>(std::move(classified).value());
			auto built = repl::buildVariableWrapper(ctx, variable_info, 404);
			ASSERT_HAS_VALUE(built, "Variable wrapper build should succeed");

			// The declaration contributes the storage of the variable, the construction of its
			// initial value is a separate function the caller sequences in statement order.
			ASSERT_EQUAL(1UL, built->hout_unit.glob_data.size());
			assertTrue(
				!built->hout_unit.functions.empty(),
				"Variable HOUT unit should contain the initializer function"
			);

			auto initializer_name
				= helios::mangler::getSimpleMangledName(ctx, built->initializer_function);
			assertTrue(
				std::string(initializer_name.strView()).find("__repl_input_wrapper_404")
					!= std::string::npos,
				"Initializer function name should include the wrapper counter"
			);

			auto variable_name = helios::mangler::getSimpleMangledName(
				ctx, built->hout_unit.glob_data.at(0)->helios_symbol
			);
			auto original_name = helios::mangler::getSimpleMangledName(
				ctx, repl::getVariableSymID(ctx, variable_info.variable_stmt)
			);
			ASSERT_EQUAL(std::string(variable_name.strView()), std::string(original_name.strView()));
		});
	}

	void testMakeExecutableHOUTUnit() {
		auto module_id = createModule("1 + 2;");

		query::utils::withContextDo([&](query::Context& ctx) {
			auto classified = repl::classifySingleStatement(ctx, module_id);
			ASSERT_HAS_VALUE(classified, "Classification should succeed");

			auto wrapped = repl::buildStatementWrapper(ctx, classified.value(), 303);
			ASSERT_HAS_VALUE(wrapped, "Wrapper build should succeed");

			auto hout_unit = repl::makeExecutableHOUTUnit(ctx, wrapped->wrapper_function);
			ASSERT_HAS_VALUE(hout_unit, "Executable HOUT unit should be created");
			assertTrue(!hout_unit->functions.empty(), "Executable HOUT unit should have a function");
			ASSERT_EQUAL(
				hout_unit->functions[0]->declaration->original_symbol,
				wrapped->wrapper_function.declaration->original_symbol
			);
		});
	}

	void testcreateSyntheticChainedStatementModule() {
		auto first_ref = repl::createSyntheticChainedStatementModule("1 + 2;", {}, 7, "repl_");
		assertTrue(first_ref->isReplModule(), "Chained module should be marked as a REPL module");
		ASSERT_EQUAL("repl_7", first_ref->getName().strView());
		ASSERT_NO_VALUE(
			first_ref->getReplModuleParent(), "First chained module should not have REPL parent"
		);
		assertTrue(first_ref->hasMainSourceFile(), "Chained module should have main source file");

		auto second_ref = repl::createSyntheticChainedStatementModule(
			"3 + 4;", first_ref->getModuleID(), 8, "script_"
		);
		assertTrue(second_ref->isReplModule(), "Second module should be marked as a REPL module");
		ASSERT_EQUAL("script_8", second_ref->getName().strView());
		ASSERT_HAS_VALUE(
			second_ref->getReplModuleParent(), "Second chained module should have REPL parent"
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
			auto hout = repl::getDefinitionHOUTUnit(ctx, module_id);
			ASSERT_HAS_VALUE(hout, "Definition HOUT should compile");
			assertTrue(
				!hout.value()->functions.empty() || !hout.value()->glob_data.empty(),
				"Definition HOUT should contain emitted elements"
			);
		});
	}

public:
	~ReplStatementHelpersTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/repl/tests/");
