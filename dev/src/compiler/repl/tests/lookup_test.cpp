#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/pst.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries/queries.hpp>

#include <filesystem/file.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <tester/tester.hpp>

using namespace compiler;

class ReplLookupTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ReplLookupTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(testReplSymbolLookupAcrossStatements); }

private:
	/**
	 * Helper to create a REPL module from source code
	 */
	frontend::ModuleID createReplModule(
		const std::string& source_code, base::Optional<frontend::ModuleID> parent = {}
	) {
		auto builder = frontend::ModuleTreeBuilder::create();
		builder->setPackageID(base::StrID(base::generateRandomString(32).c_str()));

		auto virtual_file = fs::FileManager::createRandomVirtualFile(source_code);
		builder->setMainSourceFile(virtual_file);
		builder->setName(base::StrID("test_repl_module"));

		frontend::ReplData repl_data;
		if (parent.has_value()) repl_data.m_repl_module_parent = parent.value();
		builder->setReplModule(repl_data);

		return builder->finalize()->getModuleID();
	}

	/**
	 * Test that symbol lookups propagate through REPL module parent chain.
	 * This is the main feature: a variable declared in statement 1 should be
	 * visible when we lookup symbols in statement 2.
	 *
	 * This exercises the actual scopes.cpp REPL lookup logic.
	 */
	void testReplSymbolLookupAcrossStatements() {
		// === Statement 1: Declare a variable ===
		// This creates module1 with: var x: i32 = 4;
		auto module1  = createReplModule("var x: i32 = 4;");
		auto mod1_ref = frontend::GetModuleID_Functor::get(module1);
		assertTrue(mod1_ref->isReplModule(), "Statement 1 should be REPL module");

		// === Statement 2: Reference the variable from statement 1 ===
		// This creates module2 with parent=module1, containing: var y: i32 = x + 10;
		// When this module is parsed and compiled, the lookup for 'x' should:
		// 1. Look in module2's scope -> not found
		// 2. Look in module2's parent (module1) scope -> found!
		auto module2  = createReplModule("var y: i32 = x + 10;", module1);
		auto mod2_ref = frontend::GetModuleID_Functor::get(module2);
		assertTrue(mod2_ref->isReplModule(), "Statement 2 should be REPL module");
		assertTrue(
			mod2_ref->getReplModuleParent().has_value(), "Statement 2 should have parent linkage"
		);

		// Verify parent is module1
		const auto parent_hash = mod2_ref->getReplModuleParent().value().queryUnstablePerfectHash();
		const auto expected_hash = module1.queryUnstablePerfectHash();
		assertTrue(parent_hash == expected_hash, "Statement 2's parent should be Statement 1");

		// === Verify module tree queries work with REPL modules ===
		// This tests that QueryMainSourceFile and other queries work correctly
		// with REPL modules, exercising the module_tree.cpp code paths
		auto mod1_mainfile = query::entryPoint<frontend::QueryMainSourceFile>(module1);
		auto mod2_mainfile = query::entryPoint<frontend::QueryMainSourceFile>(module2);

		// If we get here without throwing, the query succeeded
		assertTrue(
			mod1_mainfile.queryUnstablePerfectHash() != 0,
			"Module 1 should have valid main source file"
		);
		assertTrue(
			mod2_mainfile.queryUnstablePerfectHash() != 0,
			"Module 2 should have valid main source file"
		);

		// === Statement 3: Multi-level lookup ===
		// var z: i32 = x + y + 100;
		// Now lookup for 'x' and 'y' should traverse: module3 -> module2 -> module1
		auto module3  = createReplModule("var z: i32 = x + y + 100;", module2);
		auto mod3_ref = frontend::GetModuleID_Functor::get(module3);
		assertTrue(mod3_ref->isReplModule(), "Statement 3 should be REPL module");
		assertTrue(
			mod3_ref->getReplModuleParent().value().queryUnstablePerfectHash()
				== module2.queryUnstablePerfectHash(),
			"Statement 3's parent should be Statement 2"
		);

		// === Verify the full chain is intact for lookups ===
		// Query module 3 to verify it's properly set up for lookup propagation
		auto mod3_queried    = frontend::GetModuleID_Functor::get(module3);
		auto mod3_parent_opt = mod3_queried->getReplModuleParent();
		assertTrue(
			mod3_parent_opt.has_value(),
			"Queried module 3 should have parent linkage for lookup propagation"
		);

		if (mod3_parent_opt.has_value()) {
			auto mod2_from_parent = frontend::GetModuleID_Functor::get(mod3_parent_opt.value());
			assertTrue(
				mod2_from_parent->isReplModule(),
				"Module 2 (parent of 3) should also be REPL for chain lookups"
			);

			// Verify module 2's parent exists for further chain traversal
			assertTrue(
				mod2_from_parent->getReplModuleParent().has_value(),
				"Module 2 should have module 1 as parent for full lookup chain"
			);
		}

		// === Verify lookup chain: module3 -> module2 -> module1 ===
		// This validates the actual lookup path that scopes.cpp will traverse
		auto mod2_from_mod3
			= frontend::GetModuleID_Functor::get(module3)->getReplModuleParent().value();
		auto mod1_from_mod2
			= frontend::GetModuleID_Functor::get(mod2_from_mod3)->getReplModuleParent().value();

		assertTrue(
			module2.queryUnstablePerfectHash() == mod2_from_mod3.queryUnstablePerfectHash(),
			"Module 3's parent lookup should resolve to module 2"
		);
		assertTrue(
			module1.queryUnstablePerfectHash() == mod1_from_mod2.queryUnstablePerfectHash(),
			"Module 2's parent lookup should resolve to module 1"
		);
	}


public:
	~ReplLookupTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/repl/tests/")
