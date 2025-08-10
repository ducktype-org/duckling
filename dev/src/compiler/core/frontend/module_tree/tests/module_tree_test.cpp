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
		TESTER_ADD_TEST(testParseDirectoryLikeFsTree);    // moved from FsTree test
		TESTER_ADD_TEST(testVirtualFilesLikeModuleTree);  // moved from FsTree test
	}

private:
	void parseModule() {
		auto pth = fs::File(path("test_module"));
		auto mt  = ModuleTreeBuilder::create(pth, test_regex, test_regex);

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
		ASSERT_EQUAL("whoa.duck", another_module->getSourceFiles().front()->getFile().name());

		ASSERT_EQUAL(true, mt->getSubmodules().contains(base::StrID("awe")));
		auto awe_module = mt->getSubmodules()[base::StrID("awe")];
		ASSERT_EQUAL(0, awe_module->getSubmodules().size());
		ASSERT_EQUAL(0, awe_module->getSourceFiles().size());
		ASSERT_EQUAL(0, awe_module->getOtherFiles().size());
		ASSERT_EQUAL(true, awe_module->hasMainSourceFile());
		ASSERT_EQUAL("awe.dmf", awe_module->getMainSourceFile()->getFile().name());
	}

	void testModuleIDInSourceFile(base::CRef<ModuleTree> module) {
		std::cerr << "Testing module ID in source files for module: " << module->getName().strView()
				  << '\n';
		std::cerr << "Module ID: " << module->getID().asInt() << '\n';
		auto id = module->getID();
		std::cerr << "Main source file Module ID: "
				  << module->getMainSourceFile()->getModule()->getID().asInt() << '\n';
		ASSERT_EQUAL(id, module->getMainSourceFile()->getModule()->getID());
		for (auto& file: module->getSourceFiles()) ASSERT_EQUAL(id, file->getModule()->getID());
	}

	void testOtherFeatures() {
		auto pth = fs::File(path("test_module"));
		auto mt  = ModuleTreeBuilder::create(pth);

		ASSERT_EQUAL("test_module", mt->getName());
		ASSERT_EQUAL(true, mt->hasMainSourceFile());
		ASSERT_EQUAL("content123\n", mt->getMainSourceFile()->getFile().getContent().view());
		ASSERT_EQUAL(true, mt->getParentModule().empty());
		ASSERT_EQUAL(
			mt->getName(),
			mt->getSubmodules()[base::StrID("awe")]->getParentModule().value()->getName()
		);

		auto awe_module     = mt->getSubmodules()[base::StrID("awe")];
		auto another_module = mt->getSubmodules()[base::StrID("another")];
		auto awesome_module = another_module->getSubmodules()[base::StrID("awesome_module")];
		auto mod_module     = awesome_module->getSubmodules()[base::StrID("mod")];


		testModuleIDInSourceFile(awe_module);
		std::cerr << mt->prettyPrint() << '\n';
		testModuleIDInSourceFile(another_module);
		testModuleIDInSourceFile(awesome_module);
		testModuleIDInSourceFile(mod_module);
		testModuleIDInSourceFile(mt);

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

		ASSERT_EQUAL(mt->getID(), awe_module->getParentModule().value()->getID());
		ASSERT_EQUAL(mt->getID(), another_module->getParentModule().value()->getID());
		ASSERT_EQUAL(another_module->getID(), awesome_module->getParentModule().value()->getID());
		ASSERT_EQUAL(awesome_module->getID(), mod_module->getParentModule().value()->getID());
	}

	void testQueries() {
		auto pth  = fs::File(path("test_module"));
		auto root = query::entryPoint<QueryModuleTree>(pth);

		[[maybe_unused]] auto awe
			= query::entryPoint<QuerySubmodules>(root)->at(base::StrID("awe"));

		auto sources = query::entryPoint<QuerySourceFiles>(root);
		assertTrue(sources->size() == 1, "Bad source count!");

		auto main_id = sources->at(0);

		[[maybe_unused]] auto pst = query::entryPoint<QueryFilePST>(main_id);
	}

	void testParseDirectoryLikeFsTree() {
		// This test is adapted from the old FsTree parseDirectory test.
		const auto root = fs::File(path("test_directory_tree"));
		auto       mt   = ModuleTreeBuilder::create(root, test_regex, test_regex);

		// Only files with valid names/extensions are included as source or other files.
		// Check that only the correct files and directories are present as submodules or files.
		// Since ModuleTree does not expose raw file names, we check via submodules and files.

		// Check submodules (directories)
		ASSERT_EQUAL(true, mt->getSubmodules().contains(base::StrID("another_directory")));
		ASSERT_EQUAL(false, mt->getSubmodules().contains(base::StrID(".skipped_directory")));

		// Check files (source/other)
		bool found_file = false, found_file_txt = false, found_skipped = false;
		for (const auto& [ext, files]: mt->getOtherFiles()) {
			for (const auto& file: files) {
				if (file.name() == "file") found_file = true;
				if (file.name() == "file.txt") found_file_txt = true;
				if (file.name() == ".skipped_file") found_skipped = true;
			}
		}
		// Also check in source files (if any)
		for (const auto& src: mt->getSourceFiles()) {
			if (src->getFile().name() == "file") found_file = true;
			if (src->getFile().name() == "file.txt") found_file_txt = true;
			if (src->getFile().name() == ".skipped_file") found_skipped = true;
		}

		ASSERT_EQUAL(true, found_file);
		ASSERT_EQUAL(true, found_file_txt);
		ASSERT_EQUAL(false, found_skipped);

		// Check content of file.txt
		bool checked_content = false;
		for (const auto& [ext, files]: mt->getOtherFiles()) {
			for (const auto& file: files) {
				if (file.name() == "file.txt") {
					ASSERT_EQUAL("content\n", file.getContent().view());
					checked_content = true;
				}
			}
		}
		ASSERT_EQUAL(true, checked_content);

		// Check submodule's files
		auto another_dir = mt->getSubmodules().at(base::StrID("another_directory"));
		ASSERT_EQUAL(0, another_dir->getSourceFiles().size());
		ASSERT_EQUAL(0, another_dir->getOtherFiles().size());
	}

	void testVirtualFilesLikeModuleTree() {
		// This test is adapted from the old FsTree virtual files test.
		// Create a virtual root directory
		auto root = fs::FileManager::createRandomVirtualDirectory();

		// Create subdirectories and files
		auto sub_dir1 = root.createSubDirectory("subDir1");
		auto sub_dir2 = root.createSubDirectory("subDir2");
		auto file1    = root.createSubFile("File1 content", "file1.txt");
		auto file4    = sub_dir1.createSubFile("File2 content", "subDir1.dmf");
		auto file2    = sub_dir1.createSubFile("File2 content", "file2.txt");
		auto file3    = sub_dir2.createSubFile("File2 content", "subDir2.dmf");

		// Create ModuleTree from the virtual root directory
		auto mt = ModuleTreeBuilder::create(root);

		// Test root module name
		ASSERT_EQUAL(root.name(), mt->getName().strView());

		// Test submodules (directories)
		ASSERT_EQUAL(true, mt->getSubmodules().contains(base::StrID("subDir1")));
		ASSERT_EQUAL(true, mt->getSubmodules().contains(base::StrID("subDir2")));

		// Test files in root (should be in other files or source files)
		bool found_file1 = false;
		for (const auto& [ext, files]: mt->getOtherFiles()) {
			for (const auto& file: files) {
				if (file.name() == "file1.txt") {
					ASSERT_EQUAL("File1 content", file.getContent().view());
					found_file1 = true;
				}
			}
		}
		for (const auto& src: mt->getSourceFiles()) {
			if (src->getFile().name() == "file1.txt") {
				ASSERT_EQUAL("File1 content", src->getFile().getContent().view());
				found_file1 = true;
			}
		}
		ASSERT_EQUAL(true, found_file1);

		// Test files in subDir1
		auto sub1        = mt->getSubmodules().at(base::StrID("subDir1"));
		bool found_file2 = false;
		for (const auto& [ext, files]: sub1->getOtherFiles()) {
			for (const auto& file: files) {
				if (file.name() == "file2.txt") {
					ASSERT_EQUAL("File2 content", file.getContent().view());
					found_file2 = true;
				}
			}
		}
		for (const auto& src: sub1->getSourceFiles()) {
			if (src->getFile().name() == "file2.txt") {
				ASSERT_EQUAL("File2 content", src->getFile().getContent().view());
				found_file2 = true;
			}
		}
		ASSERT_EQUAL(true, found_file2);

		// Test prettyPrint() (just check output contains expected names)
		auto tree_representation = mt->prettyPrint();
		ASSERT_EQUAL(true, tree_representation.find("subDir1") != std::string::npos);
		ASSERT_EQUAL(true, tree_representation.find("file1.txt") != std::string::npos);
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/frontend/module_tree/tests/");
