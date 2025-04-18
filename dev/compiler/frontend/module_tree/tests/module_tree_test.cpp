#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <query_framework/query_entry_point.hpp>
#include <tester/tester.hpp>

using namespace compiler::frontend;

class ModuleTreeTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ModuleTreeTest

	// This regex catches anything, that starts with '.' or '$'.
	const std::regex test_regex = std::regex(R"(\..*|\$.*)");

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(parseModule);
		TESTER_ADD_TEST(testOtherFeatures);
		TESTER_ADD_TEST(testQueries);
	}

private:
	void parseModule() {
		auto pth = fs::FilePath(path("test_module"));
		auto mt  = ModuleTree::create(pth, test_regex, test_regex);

		ASSERT_EQUAL(true, mt->hasMainSourceFile());
		ASSERT_EQUAL(2, mt->getSubmodules().size());
		ASSERT_EQUAL(1, mt->getOtherFiles().size());
		ASSERT_EQUAL(1, mt->getSourceFiles().size());

		auto another_module = mt->getSubmodules()[base::StrID("another")];
		ASSERT_EQUAL(1, another_module->getSourceFiles().size());
		ASSERT_EQUAL(true, another_module->hasMainSourceFile());
		ASSERT_EQUAL(
			2, another_module->getOtherFiles().size()
		);  // 2, because there are 2 different file extensions
		ASSERT_EQUAL(2, another_module->getOtherFiles()[base::StrID(".txt")].size());
		ASSERT_EQUAL(1, another_module->getOtherFiles()[base::StrID("")].size());
		ASSERT_EQUAL(1, another_module->getSubmodules().size());
		ASSERT_EQUAL("whoa.duck", another_module->getSourceFiles().front().path.name());

		ASSERT_EQUAL(true, mt->getSubmodules().contains(base::StrID("awe")));
		auto awe_module = mt->getSubmodules()[base::StrID("awe")];
		ASSERT_EQUAL(0, awe_module->getSubmodules().size());
		ASSERT_EQUAL(0, awe_module->getSourceFiles().size());
		ASSERT_EQUAL(0, awe_module->getOtherFiles().size());
		ASSERT_EQUAL(true, awe_module->hasMainSourceFile());
		ASSERT_EQUAL("awe.dmf", awe_module->getMainSourceFile().path.name());
	}

	void testModuleIDInSourceFile(const ModuleTree& module) {
		auto id = module.getID();
		ASSERT_EQUAL(id, module.getMainSourceFile().linked_module);
		for (auto& file: module.getSourceFiles()) ASSERT_EQUAL(id, file.linked_module);
	}

	void testOtherFeatures() {
		auto pth = fs::FilePath(path("test_module"));
		auto mt  = ModuleTree::create(pth);

		ASSERT_EQUAL("test_module", mt->getName());
		ASSERT_EQUAL(true, mt->hasMainSourceFile());
		ASSERT_EQUAL("content123\n", mt->getMainSourceFile().path.getContent().view());
		ASSERT_EQUAL(true, mt->getParentModule().empty());
		ASSERT_EQUAL(
			mt->getName(), mt->getSubmodules()[base::StrID("awe")]->getParentModule()->getName()
		);

		auto awe_module     = mt->getSubmodules()[base::StrID("awe")];
		auto another_module = mt->getSubmodules()[base::StrID("another")];
		auto awesome_module = another_module->getSubmodules()[base::StrID("awesome_module")];
		auto mod_module     = awesome_module->getSubmodules()[base::StrID("mod")];

		testModuleIDInSourceFile(*awe_module);
		testModuleIDInSourceFile(*another_module);
		testModuleIDInSourceFile(*awesome_module);
		testModuleIDInSourceFile(*mod_module);
		testModuleIDInSourceFile(*mt);

		assertTrue(not mt->getParentModule().has_value(), "Root module has a parent");
		assertTrue(
			awe_module->getParentModule().has_value(), "Non-root module does not have a parent (1)"
		);
		assertTrue(
			another_module->getParentModule().has_value(),
			"Non-root module does not have a parent (2)"
		);
		assertTrue(
			awesome_module->getParentModule().has_value(),
			"Non-root module does not have a parent (3)"
		);
		assertTrue(
			mod_module->getParentModule().has_value(), "Non-root module does not have a parent (4)"
		);

		ASSERT_EQUAL(mt->getID(), awe_module->getParentModule().value().getID());
		ASSERT_EQUAL(mt->getID(), another_module->getParentModule().value().getID());
		ASSERT_EQUAL(another_module->getID(), awesome_module->getParentModule().value().getID());
		ASSERT_EQUAL(awesome_module->getID(), mod_module->getParentModule().value().getID());
	}

	void testQueries() {
		auto pth  = fs::FilePath(path("test_module"));
		auto root = query::entryPoint<QueryModuleTree>(pth);

		[[maybe_unused]] auto awe = query::entryPoint<QuerySubmodules>(root)->at(base::StrID("awe"));

		auto sources = query::entryPoint<QuerySourceFiles>(root);
		assertTrue(sources->size() == 1, "Bad source count!");

		auto main_id = sources->at(0);

		[[maybe_unused]] auto pst = query::entryPoint<QueryFilePST>(main_id);
	}
};

TESTER_COMMON_MAIN("/compiler/frontend/module_tree/tests/");
