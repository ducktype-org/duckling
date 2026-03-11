#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>

#include <filesystem/file.hpp>
#include <tester/tester.hpp>

using namespace compiler;

class ReplLookupTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ReplLookupTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testReplModuleCreation);
		TESTER_ADD_TEST(testReplParentLinkage);
		TESTER_ADD_TEST(testReplModuleChaining);
		TESTER_ADD_TEST(testDeepModuleChaining);
	}

private:
	/**
	 * Helper to create a REPL module from source code
	 */
	frontend::ModuleID createReplModule(
		const std::string& source_code, base::Optional<frontend::ModuleID> parent = {}
	) {
		auto builder = frontend::ModuleTreeBuilder::create();
		builder->setPackageID(base::generateRandomString(32));

		auto virtual_file = fs::FileManager::createRandomVirtualFile(source_code);
		builder->setMainSourceFile(virtual_file);
		builder->setName(base::StrID("test_repl_module"));

		frontend::ReplData repl_data;
		if (parent.has_value()) repl_data.m_repl_module_parent = parent.value();
		builder->setReplModule(repl_data);

		return builder->finalize()->getModuleID();
	}

	/**
	 * Test that REPL modules are properly created and marked as such
	 */
	void testReplModuleCreation() {
		auto module     = createReplModule("fun foo() = { 42 }");
		auto module_ref = frontend::GetModuleID_Functor::get(module);

		assertTrue(module_ref->isReplModule(), "Module should be marked as REPL module");
	}

	/**
	 * Test that parent module linkage is properly established
	 */
	void testReplParentLinkage() {
		// Create first REPL module (no parent)
		auto module1     = createReplModule("fun foo() = { 42 }");
		auto module1_ref = frontend::GetModuleID_Functor::get(module1);

		assertTrue(module1_ref->isReplModule(), "Module 1 should be marked as REPL module");
		assertTrue(
			!module1_ref->getReplModuleParent().has_value(),
			"First REPL module should have no parent"
		);

		// Create second REPL module (with parent)
		auto module2     = createReplModule("fun bar() = { 100 }", module1);
		auto module2_ref = frontend::GetModuleID_Functor::get(module2);

		assertTrue(module2_ref->isReplModule(), "Module 2 should be marked as REPL module");
		assertTrue(
			module2_ref->getReplModuleParent().has_value(), "Second REPL module should have parent"
		);
		ASSERT_EQUAL(
			module1.queryUnstablePerfectHash(),
			module2_ref->getReplModuleParent().value().queryUnstablePerfectHash()
		);
	}

	/**
	 * Test that REPL modules can be chained (multiple statements)
	 */
	void testReplModuleChaining() {
		// Simulate three REPL statements creating a chain
		auto stmt1 = createReplModule("var x: i32 = 5;");
		auto stmt2 = createReplModule("var y: i32 = 10;", stmt1);
		auto stmt3 = createReplModule("var z: i32 = x + y;", stmt2);

		auto stmt1_ref = frontend::GetModuleID_Functor::get(stmt1);
		auto stmt2_ref = frontend::GetModuleID_Functor::get(stmt2);
		auto stmt3_ref = frontend::GetModuleID_Functor::get(stmt3);

		// Verify stmt2's parent is stmt1
		assertTrue(stmt2_ref->getReplModuleParent().has_value(), "stmt2 should have parent");
		ASSERT_EQUAL(
			stmt1.queryUnstablePerfectHash(),
			stmt2_ref->getReplModuleParent().value().queryUnstablePerfectHash()
		);

		// Verify stmt3's parent is stmt2
		assertTrue(stmt3_ref->getReplModuleParent().has_value(), "stmt3 should have parent");
		ASSERT_EQUAL(
			stmt2.queryUnstablePerfectHash(),
			stmt3_ref->getReplModuleParent().value().queryUnstablePerfectHash()
		);

		// Verify chain: stmt1 has no parent, stmt2 -> stmt1, stmt3 -> stmt2
		assertTrue(
			!stmt1_ref->getReplModuleParent().has_value(),
			"stmt1 (first statement) should have no parent"
		);
	}

	/**
	 * Test deep module chaining with 10 modules
	 */
	void testDeepModuleChaining() {
		const auto                      chain_length = 10;
		std::vector<frontend::ModuleID> chain;

		// Create a long chain of modules
		for (auto i = 0; i < chain_length; ++i) {
			std::string source = "var v" + std::to_string(i) + ": i32 = " + std::to_string(i) + ";";
			if (i == 0)
				chain.push_back(createReplModule(source));
			else
				chain.push_back(createReplModule(source, chain[static_cast<size_t>(i - 1)]));
		}

		// Verify entire chain integrity
		for (auto i = 0; i < chain_length; ++i) {
			auto mod_ref = frontend::GetModuleID_Functor::get(chain[static_cast<size_t>(i)]);
			assertTrue(
				mod_ref->isReplModule(), "Module at index " + std::to_string(i) + " should be REPL"
			);

			if (i == 0) {
				// First module has no parent
				assertTrue(
					!mod_ref->getReplModuleParent().has_value(), "First module should have no parent"
				);
			} else {
				// All other modules should point to previous module
				assertTrue(
					mod_ref->getReplModuleParent().has_value(),
					"Module at index " + std::to_string(i) + " should have parent"
				);
				ASSERT_EQUAL(
					chain[static_cast<size_t>(i - 1)].queryUnstablePerfectHash(),
					mod_ref->getReplModuleParent().value().queryUnstablePerfectHash()
				);
			}
		}
	}

public:
	~ReplLookupTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/repl/tests/")
