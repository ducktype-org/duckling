#include <tester/tester.hpp>
#include "frontend/module_tree/module_tree.hpp"

using namespace compiler::frontend;

class ModuleTreeTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ModuleTreeTest

	// This regex catches anything, that starts with '.' or '$'.
	const std::regex test_regex = std::regex(R"(\..*|\$.*)");

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("frontend::ModuleTree test") {
		TESTER_ADD_TEST(parseModule);
		TESTER_ADD_TEST(testOtherFeatures);
	}

private:
	void parseModule() {
		auto pth = fs::FilePath(path("test_module"));
		auto mt  = ModuleTree::create(pth, test_regex, test_regex);

		ASSERT_EQUAL(true, mt->hasMainSourceFile());
		ASSERT_EQUAL(2, mt->getSubmodules().size());
		ASSERT_EQUAL(1, mt->getOtherFiles().size());
		ASSERT_EQUAL(1, mt->getSourceFiles().size());

		auto another_module = mt->getSubmodules()["another"];
		ASSERT_EQUAL(1, another_module->getSourceFiles().size());
		ASSERT_EQUAL(true, another_module->hasMainSourceFile());
		ASSERT_EQUAL(
			2, another_module->getOtherFiles().size()
		);  // 2, because there are 2 different file extensions
		ASSERT_EQUAL(2, another_module->getOtherFiles()[".txt"].size());
		ASSERT_EQUAL(1, another_module->getOtherFiles()[""].size());
		ASSERT_EQUAL(1, another_module->getSubmodules().size());
		ASSERT_EQUAL("whoa.rift", another_module->getSourceFiles().front().name());

		ASSERT_EQUAL(true, mt->getSubmodules().contains("awe"));
		auto awe_module = mt->getSubmodules()["awe"];
		ASSERT_EQUAL(0, awe_module->getSubmodules().size());
		ASSERT_EQUAL(0, awe_module->getSourceFiles().size());
		ASSERT_EQUAL(0, awe_module->getOtherFiles().size());
		ASSERT_EQUAL(true, awe_module->hasMainSourceFile());
		ASSERT_EQUAL("awe.rmf", awe_module->getMainSourceFile().name());
	}

	void testOtherFeatures() {
		auto pth = fs::FilePath(path("test_module"));
		auto mt  = ModuleTree::create(pth);

		ASSERT_EQUAL("test_module", mt->getName());
		ASSERT_EQUAL(true, mt->hasMainSourceFile());
		ASSERT_EQUAL("content123\n", mt->getMainSourceFile().getContent().view());
		ASSERT_EQUAL(true, mt->getParentModule().empty());
		ASSERT_EQUAL(mt->getName(), mt->getSubmodules()["awe"]->getParentModule()->getName());
	}
};

TESTER_COMMON_MAIN("/RiftCompiler/frontend/module_tree/tests/");
