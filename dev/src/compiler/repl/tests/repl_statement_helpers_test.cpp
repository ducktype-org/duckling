#include <driver/repl_utils/repl_statement_helpers.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/packages/packages.hpp>
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
		TESTER_ADD_TEST(testClassifyRejectsNonSingleInput);
		TESTER_ADD_TEST(testClassifyRejectsSingleActionStatements);
		TESTER_ADD_TEST(testBuildExpressionWrapper);
		TESTER_ADD_TEST(testBuildInstructionWrapper);
		TESTER_ADD_TEST(testBuildWrapperRejectsDefinition);
		TESTER_ADD_TEST(testMakeExecutableHOUTUnit);
		TESTER_ADD_TEST(testCreateSyntheticChainedStatementModule);
		TESTER_ADD_TEST(testCreateSyntheticChainedStatementModulePreservesPackageID);
		TESTER_ADD_TEST(testGetRelativeModuleFromScriptChain);
		TESTER_ADD_TEST(testGetRelativeModuleFromScriptChainWithoutFixtures);
		TESTER_ADD_TEST(testGetRelativeModuleLegacyAncestorFallbackFromScriptChain);
		TESTER_ADD_TEST(testGetStatementModuleName);
		TESTER_ADD_TEST(testGetDefinitionHOUTUnit);
	}

private:
	static frontend::ModuleID createModule(std::string_view code) {
		return frontend::createModuleTreeFromContents(code, "test_pkg");
	}

	static base::Ref<frontend::ModuleTree> createNamedModule(
		std::string_view package_id,
		std::string_view module_name,
		std::string_view source = "fun x() = {}"
	) {
		auto builder = frontend::ModuleTreeBuilder::create();
		builder->setPackageID(base::StrID(std::string(package_id).c_str()));
		builder->setName(base::StrID(std::string(module_name).c_str()));
		builder->setMainSourceFile(fs::FileManager::createRandomVirtualFile(source));
		return builder->finalize();
	}

	void testClassifySingleExpression() {
		auto module_id = createModule("1 + 2;");

		query::utils::withContextDo([&](query::Context& ctx) {
			auto result = repl::classifySingleStatement(ctx, module_id);
			assertTrue(result.has_value(), "Expression classification should succeed");
			assertTrue(
				std::holds_alternative<repl::ExpressionSingleStatementInfo>(*result),
				"Expected expression variant"
			);
		});
	}

	void testClassifyAssignmentExpressionAsInstruction() {
		auto assignment_test = [&](std::string_view code) {
			auto module_id = createModule(code);

			query::utils::withContextDo([&](query::Context& ctx) {
				auto result = repl::classifySingleStatement(ctx, module_id);
				assertTrue(result.has_value(), "Assignment classification should succeed");
				assertTrue(
					std::holds_alternative<repl::InstructionSingleStatementInfo>(*result),
					"Expected assignment ExprStmt to be routed as instruction"
				);
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
			assertTrue(result.has_value(), "Instruction classification should succeed");
			assertTrue(
				std::holds_alternative<repl::InstructionSingleStatementInfo>(*result),
				"Expected instruction variant"
			);
		});
	}

	void testClassifySingleDefinition() {
		auto module_id = createModule("fun foo() = {}");

		query::utils::withContextDo([&](query::Context& ctx) {
			auto result = repl::classifySingleStatement(ctx, module_id);
			assertTrue(result.has_value(), "Definition classification should succeed");
			assertTrue(
				std::holds_alternative<repl::DefinitionSingleStatementInfo>(*result),
				"Expected definition variant"
			);
		});
	}

	void testClassifyRejectsNonSingleInput() {
		auto module_id = createModule("1 + 2;\n3 + 4;");

		query::utils::withContextDo([&](query::Context& ctx) {
			auto result = repl::classifySingleStatement(ctx, module_id);
			assertTrue(!result.has_value(), "Expected error for non-single-statement input");
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
				assertTrue(!result.has_value(), "Expected action statement to be unclassified");
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

	void testCreateSyntheticChainedStatementModule() {
		auto first_ref = repl::createSyntheticChainedStatementModule("1 + 2;", {}, 7, "repl_");
		assertTrue(first_ref->isReplModule(), "Chained module should be marked as a REPL module");
		ASSERT_EQUAL("repl_7", first_ref->getName().strView());
		assertTrue(
			!first_ref->getReplModuleParent().has_value(),
			"First chained module should not have REPL parent"
		);
		assertTrue(first_ref->hasMainSourceFile(), "Chained module should have main source file");

		auto second_ref = repl::createSyntheticChainedStatementModule(
			"3 + 4;", first_ref->getModuleID(), 8, "script_"
		);
		assertTrue(second_ref->isReplModule(), "Second module should be marked as a REPL module");
		ASSERT_EQUAL("script_8", second_ref->getName().strView());
		assertTrue(
			second_ref->getReplModuleParent().has_value(),
			"Second chained module should have REPL parent"
		);
		ASSERT_EQUAL(second_ref->getReplModuleParent().value(), first_ref->getModuleID());
	}

	void testCreateSyntheticChainedStatementModulePreservesPackageID() {
		auto root_module     = createModule("fun root_value() = {};");
		auto root_package_id = getModuleRef(root_module)->getPackage().illegalAccess().getID();

		auto first_ref
			= repl::createSyntheticChainedStatementModule("1 + 2;", root_module, 7, "script_");
		ASSERT_EQUAL(root_package_id, first_ref->getPackage().illegalAccess().getID());

		auto second_ref = repl::createSyntheticChainedStatementModule(
			"3 + 4;", first_ref->getModuleID(), 8, "script_"
		);
		ASSERT_EQUAL(
			first_ref->getPackage().illegalAccess().getID(),
			second_ref->getPackage().illegalAccess().getID()
		);
	}

	void testGetRelativeModuleFromScriptChain() {
		auto root = compiler::frontend::createModuleTreeWithRandomPackageID(
			fs::File(path("../../core/frontend/module_tree/tests/test_module"))
		);
		auto script_module
			= repl::createSyntheticChainedStatementModule("import awe as a;", root, 1, "script_");

		query::utils::withContextDo([&](query::Context& ctx) {
			auto direct_child = frontend::getRelativeModule(
				ctx, script_module->getModuleID(), { base::StrID("awe") }
			);
			assertTrue(
				direct_child.has_value(), "Script chain should resolve direct package children"
			);
			ASSERT_EQUAL("awe", frontend::moduleName(direct_child.value()).strView());

			auto nested_child = frontend::getRelativeModule(
				ctx,
				script_module->getModuleID(),
				{ base::StrID("another"), base::StrID("awesome_module") }
			);
			assertTrue(
				nested_child.has_value(), "Script chain should resolve nested package children"
			);
			ASSERT_EQUAL("awesome_module", frontend::moduleName(nested_child.value()).strView());
		});
	}

	void testGetRelativeModuleFromScriptChainWithoutFixtures() {
		auto root = createNamedModule("pkg_lookup", "pkgroot");

		auto alpha = createNamedModule("pkg_lookup", "alpha");
		auto beta  = createNamedModule("pkg_lookup", "beta");
		auto gamma = createNamedModule("pkg_lookup", "gamma");

		frontend::ModuleTreeModifier::addSubmodule(beta, gamma);
		frontend::ModuleTreeModifier::addSubmodule(alpha, beta);
		frontend::ModuleTreeModifier::addSubmodule(root, alpha);

		auto chain_1 = repl::createSyntheticChainedStatementModule(
			"1 + 2;", root->getModuleID(), 1, "script_"
		);
		auto chain_2 = repl::createSyntheticChainedStatementModule(
			"3 + 4;", chain_1->getModuleID(), 2, "script_"
		);

		query::utils::withContextDo([&](query::Context& ctx) {
			auto resolved_alpha
				= frontend::getRelativeModule(ctx, chain_2->getModuleID(), { base::StrID("alpha") });
			assertTrue(
				resolved_alpha.has_value(),
				"Script chain should resolve ancestor child module from in-memory package tree"
			);
			ASSERT_EQUAL("alpha", frontend::moduleName(resolved_alpha.value()).strView());

			auto resolved_gamma = frontend::getRelativeModule(
				ctx,
				chain_2->getModuleID(),
				{ base::StrID("alpha"), base::StrID("beta"), base::StrID("gamma") }
			);
			assertTrue(
				resolved_gamma.has_value(),
				"Script chain should resolve nested descendant path from ancestor child"
			);
			ASSERT_EQUAL("gamma", frontend::moduleName(resolved_gamma.value()).strView());

			auto missing = frontend::getRelativeModule(
				ctx, chain_2->getModuleID(), { base::StrID("does_not_exist") }
			);
			assertTrue(!missing.has_value(), "Missing module path should not resolve");
		});
	}

	void testGetRelativeModuleLegacyAncestorFallbackFromScriptChain() {
		auto root = createNamedModule("pkg_lookup_fallback", "pkgroot");
		auto chain_1
			= repl::createSyntheticChainedStatementModule("1;", root->getModuleID(), 1, "script_");
		auto chain_2 = repl::createSyntheticChainedStatementModule(
			"2;", chain_1->getModuleID(), 2, "script_"
		);

		query::utils::withContextDo([&](query::Context& ctx) {
			auto resolved_root = frontend::getRelativeModule(
				ctx, chain_2->getModuleID(), { base::StrID("pkgroot") }
			);
			assertTrue(
				resolved_root.has_value(),
				"Script chain should resolve an ancestor by name via legacy fallback"
			);
			ASSERT_EQUAL(root->getModuleID(), resolved_root.value());
		});
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
