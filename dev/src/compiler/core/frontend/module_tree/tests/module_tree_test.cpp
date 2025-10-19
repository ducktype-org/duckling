#include <frontend/module_tree/functors.hpp>
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
		TESTER_ADD_TEST(testParseDirectoryLikeFsTree);
		TESTER_ADD_TEST(testVirtualFilesLikeModuleTree);
		TESTER_ADD_TEST(testModuleTreeModifierVariants);
		TESTER_ADD_TEST(testManualModuleTreeBuilder);
		TESTER_ADD_TEST(testModuleLoop);
		TESTER_ADD_TEST(testComponentHash);
		TESTER_ADD_TEST(testPrintModuleTree);
	}

private:
	void testPrintModuleTree() {
		// Test with valid name
		auto temp_file            = fs::FileManager::createRandomVirtualFile("fn main() {}\n");
		auto dummy_module_builder = ModuleTreeBuilder::create();
		dummy_module_builder->setName(base::StrID("dummy"));
		dummy_module_builder->setPackageID("dummy_package");
		dummy_module_builder->setMainSourceFile(temp_file);
		auto dummy_module = dummy_module_builder->finalize();
		auto module_id    = dummy_module->getModuleID();
		auto tree_str     = printModuleTree(module_id);
		assertTrue(
			tree_str.find("dummy") != std::string::npos, "Module tree should contain module name"
		);
		fs::FileManager::deleteFile(temp_file);

		// Test with no name (bad name)
		auto temp_file2      = fs::FileManager::createRandomVirtualFile("fn main() {}\n");
		auto no_name_builder = ModuleTreeBuilder::create();
		no_name_builder->setPackageID("no_name_package");
		// do NOT set name
		no_name_builder->setMainSourceFile(temp_file2);
		auto        no_name_module = no_name_builder->finalize();
		auto        no_name_id     = no_name_module->getModuleID();
		auto        tree_str2      = printModuleTree(no_name_id);
		std::string hash_str       = std::to_string(no_name_id.queryUnstablePerfectHash());
		assertTrue(
			tree_str2.find("id:") != std::string::npos, "Output should contain 'id:' for bad name"
		);
		assertTrue(
			tree_str2.find(hash_str) != std::string::npos, "Output should contain module hash"
		);
		fs::FileManager::deleteFile(temp_file2);
	}

	void parseModule() {
		auto pth = fs::File(path("test_module"));
		auto mt  = ModuleTreeBuilder::create(pth, "test_package_id", test_regex, test_regex);

		ASSERT_TRUE(mt->hasMainSourceFile());
		ASSERT_EQUAL(2, mt->getSubmodules().size());
		ASSERT_EQUAL(1, mt->getOtherFiles().size());
		ASSERT_EQUAL(1, mt->getSourceFiles().size());

		auto another_module = mt->getSubmodules()[base::StrID("another")];
		ASSERT_EQUAL(1, another_module->getSourceFiles().size());
		ASSERT_TRUE(another_module->hasMainSourceFile());
		ASSERT_EQUAL(
			2, another_module->getOtherFiles().size()
		);  // 2, because there are 2 different file extensions
		ASSERT_EQUAL(2, another_module->getOtherFiles()[base::StrID(".txt")].size());
		ASSERT_EQUAL(1, another_module->getOtherFiles()[base::StrID("")].size());
		ASSERT_EQUAL(1, another_module->getSubmodules().size());
		ASSERT_EQUAL("whoa.duck", another_module->getSourceFiles().front()->getFile().name());

		ASSERT_TRUE(mt->getSubmodules().contains(base::StrID("awe")));
		auto awe_module = mt->getSubmodules()[base::StrID("awe")];
		ASSERT_EQUAL(0, awe_module->getSubmodules().size());
		ASSERT_EQUAL(0, awe_module->getSourceFiles().size());
		ASSERT_EQUAL(0, awe_module->getOtherFiles().size());
		ASSERT_TRUE(awe_module->hasMainSourceFile());
		ASSERT_EQUAL("awe.dmf", awe_module->getMainSourceFile()->getFile().name());
	}

	void testModuleIDInSourceFile(base::Ref<ModuleTree> module) {
		std::cerr << "Testing module ID in source files for module: " << module->getName().strView()
				  << '\n';
		auto id = module->getModuleID();
		std::cerr << "Module id: " << id.queryUnstablePerfectHash() << '\n';
		std::cerr << "Main source file Module ID: "
				  << module->getMainSourceFile()->getModule().queryUnstablePerfectHash() << '\n';
		ASSERT_EQUAL(
			id.queryUnstablePerfectHash(),
			module->getMainSourceFile()->getModule().queryUnstablePerfectHash()
		);
		for (auto& file: module->getSourceFiles())
			ASSERT_EQUAL(
				id.queryUnstablePerfectHash(), file->getModule().queryUnstablePerfectHash()
			);
	}

	void testOtherFeatures() {
		auto pth = fs::File(path("test_module"));
		auto mt  = ModuleTreeBuilder::create(pth, "another_package_id");

		ASSERT_EQUAL("test_module", mt->getName());
		ASSERT_TRUE(mt->hasMainSourceFile());
		ASSERT_EQUAL("content123\n", mt->getMainSourceFile()->getFile().getContent().view());
		ASSERT_TRUE(mt->getParentModule().empty());
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

		ASSERT_EQUAL(mt->getModuleID(), awe_module->getParentModule().value()->getModuleID());
		ASSERT_EQUAL(mt->getModuleID(), another_module->getParentModule().value()->getModuleID());
		ASSERT_EQUAL(
			another_module->getModuleID(), awesome_module->getParentModule().value()->getModuleID()
		);
		ASSERT_EQUAL(
			awesome_module->getModuleID(), mod_module->getParentModule().value()->getModuleID()
		);
	}

	void testQueries() {
		auto pth  = fs::File(path("test_module"));
		auto root = compiler::frontend::createModuleTreeWithRandomPackageID(pth);

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
		auto       mt
			= ModuleTreeBuilder::create(root, "test_package_id23423423", test_regex, test_regex);

		// Only files with valid names/extensions are included as source or other files.
		// Check that only the correct files and directories are present as submodules or files.
		// Since ModuleTree does not expose raw file names, we check via submodules and files.

		// Check submodules (directories)
		ASSERT_TRUE(mt->getSubmodules().contains(base::StrID("another_directory")));
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

		ASSERT_TRUE(found_file);
		ASSERT_TRUE(found_file_txt);
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
		ASSERT_TRUE(checked_content);

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
		auto mt = ModuleTreeBuilder::create(root, "virtual_package_id1312");

		// Test root module name
		ASSERT_EQUAL(root.name(), mt->getName().strView());

		// Test submodules (directories)
		ASSERT_TRUE(mt->getSubmodules().contains(base::StrID("subDir1")));
		ASSERT_TRUE(mt->getSubmodules().contains(base::StrID("subDir2")));

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
		ASSERT_TRUE(found_file1);

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
		ASSERT_TRUE(found_file2);

		// Test prettyPrint() (just check output contains expected names)
		auto tree_representation = mt->prettyPrint();
		ASSERT_TRUE(tree_representation.find("subDir1") != std::string::npos);
		ASSERT_TRUE(tree_representation.find("file1.txt") != std::string::npos);
	}

	void testModuleTreeModifierVariants() {
		// Create a virtual root directory and files for testing
		auto root_dir = fs::FileManager::createRandomVirtualDirectory();
		auto file1    = root_dir.createSubFile("main content", "main.dmf");
		auto file2    = root_dir.createSubFile("src content", "src1.duck");
		auto file3    = root_dir.createSubFile("other content", "other.txt");
		auto file4    = root_dir.createSubFile("other2 content", "other2.md");

		// Build initial module tree
		auto mt = ModuleTreeBuilder::create(root_dir, "modifier_test_package_id65");
		// Test addSourceFile
		auto new_src = root_dir.createSubFile("new src", "newsrc.duck");
		ModuleTreeModifier::addSourceFile(mt, new_src);
		bool found_newsrc = false;
		for (auto& sf: mt->getSourceFiles())
			if (sf->getFile().name() == "newsrc.duck") found_newsrc = true;
		ASSERT_TRUE(found_newsrc);

		// Test removeSourceFile
		auto src_to_remove = mt->getSourceFiles().front();
		ModuleTreeModifier::removeSourceFile(src_to_remove);
		bool still_present = false;
		for (auto& sf: mt->getSourceFiles())
			if (sf == src_to_remove) still_present = true;
		ASSERT_EQUAL(false, still_present);

		// Test setMainSourceFile (remove first if exists)
		if (mt->hasMainSourceFile()) {
			ModuleTreeModifier::removeMainSourceFile(mt);
			ASSERT_EQUAL(false, mt->hasMainSourceFile());
		}
		auto new_main = root_dir.createSubFile("main2", "main2.dmf");
		ModuleTreeModifier::setMainSourceFile(mt, new_main);
		ASSERT_EQUAL("main2.dmf", mt->getMainSourceFile()->getFile().name());

		// Test removeMainSourceFile explicitly
		ModuleTreeModifier::removeMainSourceFile(mt);
		ASSERT_EQUAL(false, mt->hasMainSourceFile());

		// Test addOtherFile
		auto other_file = root_dir.createSubFile("other3", "other3.txt");
		ModuleTreeModifier::addOtherFile(mt, other_file);
		bool found_other3 = false;
		for (auto& f: mt->getOtherFiles()[base::StrID(".txt")])
			if (f.name() == "other3.txt") found_other3 = true;
		ASSERT_TRUE(found_other3);

		// Test removeOtherFile
		ModuleTreeModifier::removeOtherFile(mt, other_file);
		bool still_other3 = false;
		for (auto& f: mt->getOtherFiles()[base::StrID(".txt")])
			if (f.name() == "other3.txt") still_other3 = true;
		ASSERT_EQUAL(false, still_other3);

		// Test addSubmodule and setParent/removeParent
		auto sub_dir = root_dir.createSubDirectory("submod");
		auto sub_mod = ModuleTreeBuilder::create(sub_dir, "modifier_test_package_id65");
		ModuleTreeModifier::addSubmodule(mt, sub_mod);
		ASSERT_TRUE(mt->getSubmodules().contains(sub_mod->getName()));
		// Remove parent
		ModuleTreeModifier::removeParent(sub_mod);
		ASSERT_EQUAL(false, sub_mod->getParentModule().has_value());
		// Set parent again
		ModuleTreeModifier::setParent(sub_mod, mt);
		ASSERT_EQUAL(mt, sub_mod->getParentModule().value());

		// Test removeModule
		ModuleTreeModifier::removeModule(sub_mod);
		ASSERT_EQUAL(false, mt->getSubmodules().contains(base::StrID("submod")));

		// Test fileModified (should not throw)
		// auto src_file
		// 	= mt->getSourceFiles().empty() ? new_src : mt->getSourceFiles().front();
		// ModuleTreeModifier::fileModified(src_file);

		// Test removeModule with a module that has source files and main source file
		{
			// Use unique variable names to avoid shadowing
			auto removable_sub_dir   = root_dir.createSubDirectory("removable");
			auto removable_main_file = removable_sub_dir.createSubFile("main", "removable.dmf");
			auto removable_src_file1 = removable_sub_dir.createSubFile("src1", "src1.duck");
			auto removable_src_file2 = removable_sub_dir.createSubFile("src2", "src2.duck");
			auto removable_sub_mod
				= ModuleTreeBuilder::create(removable_sub_dir, "modifier_test_package_id65");

			// Add as submodule
			ModuleTreeModifier::addSubmodule(mt, removable_sub_mod);
			ASSERT_TRUE(mt->getSubmodules().contains(removable_sub_mod->getName()));

			// Check that main and source files exist in SourceFile::file_map
			ASSERT_TRUE(removable_sub_mod->hasMainSourceFile());
			auto removable_main_id = removable_sub_mod->getMainSourceFile();
			ASSERT_TRUE(removable_main_id->getModule() == removable_sub_mod->getModuleID());
			for (auto& sf: removable_sub_mod->getSourceFiles())
				ASSERT_TRUE(sf->getModule() == removable_sub_mod->getModuleID());

			// Remove the submodule
			ModuleTreeModifier::removeModule(removable_sub_mod);
			ASSERT_EQUAL(false, mt->getSubmodules().contains(base::StrID("removable")));
		}
	}

	void testManualModuleTreeBuilder() {
		// Manually build a module tree using ModuleTreeBuilder (not from filesystem)
		auto builder = ModuleTreeBuilder::create();

		// Set name
		builder->setName(base::StrID("manual_mod"));
		builder->setPackageID("manual_package_id456");

		// Create virtual files
		auto root_dir   = fs::FileManager::createRandomVirtualDirectory();
		auto main_file  = root_dir.createSubFile("main", "manual_mod.dmf");
		auto src_file1  = root_dir.createSubFile("src1", "src1.duck");
		auto src_file2  = root_dir.createSubFile("src2", "src2.duck");
		auto other_file = root_dir.createSubFile("other", "other.txt");

		// Set main source file
		builder->setMainSourceFile(main_file);
		// Add source files
		builder->addSourceFile(src_file1);
		builder->addSourceFile(src_file2);
		// Add other file
		builder->addOtherFile(other_file);

		// Add submodule
		auto sub_dir     = root_dir.createSubDirectory("subdir");
		auto sub_builder = ModuleTreeBuilder::create();
		sub_builder->setPackageID("manual_package_id456");
		sub_builder->setName(base::StrID("subdir"));
		auto sub_main = sub_dir.createSubFile("submain", "subdir.dmf");
		sub_builder->setMainSourceFile(sub_main);

		// Test isValid before finalize
		ASSERT_TRUE(!builder->isFinalized());
		ASSERT_TRUE(!sub_builder->isFinalized());

		auto sub_mod = sub_builder->finalize();
		ASSERT_EQUAL(false, !sub_builder->isFinalized());

		builder->addSubmodule(sub_mod);

		// Test setParent
		auto parent_builder = ModuleTreeBuilder::create();
		parent_builder->setName(base::StrID("parent_mod"));
		parent_builder->setPackageID("manual_package_id456");
		auto parent_file = root_dir.createSubFile("parent", "parent_mod.dmf");
		parent_builder->setMainSourceFile(parent_file);
		auto parent_mod = parent_builder->finalize();
		builder->setParent(parent_mod);

		// Finalize
		auto mt = builder->finalize();
		ASSERT_EQUAL(false, !builder->isFinalized());

		// Check structure
		ASSERT_EQUAL("manual_mod", mt->getName().strView());
		ASSERT_TRUE(mt->hasMainSourceFile());
		ASSERT_TRUE(parent_mod->getSubmodules().contains(mt->getName()));
		ASSERT_EQUAL(2, mt->getSourceFiles().size());
		ASSERT_EQUAL(1, mt->getOtherFiles().size());
		ASSERT_EQUAL(1, mt->getSubmodules().size());
		ASSERT_EQUAL("subdir", mt->getSubmodules().begin()->first.strView());
		ASSERT_TRUE(mt->getSubmodules().begin()->second->hasMainSourceFile());
		// Check parent
		ASSERT_TRUE(mt->getParentModule().has_value());
		ASSERT_EQUAL("parent_mod", mt->getParentModule().value()->getName().strView());
	}

	void testModuleLoop() {
		auto root = fs::FileManager::createRandomVirtualDirectory();
		auto md1  = root.createSubDirectory("md1");
		(void) md1.createSubFile("main1", "md1.dmf");
		auto md2 = root.createSubDirectory("md2");
		(void) md2.createSubFile("main2", "md2.dmf");

		auto mt1 = ModuleTreeBuilder::create(md1, "md1_package_id");
		auto mt2 = ModuleTreeBuilder::create(md2, "md1_package_id");

		ModuleTreeModifier::setParent(mt1, mt2);
		bool exception_thrown = false;
		try {
			ModuleTreeModifier::setParent(mt2, mt1);
		} catch (const std::exception& e) { exception_thrown = true; }
		ASSERT_TRUE(exception_thrown);
	}

	void testComponentHash() {
		// Create virtual directory with main module and two submodules
		auto random = fs::FileManager::createRandomVirtualDirectory();
		auto root   = random.createSubDirectory("root");
		(void) root.createSubFile("root", "root.dmf");

		auto sd1 = root.createSubDirectory("sub1");
		(void) sd1.createSubFile("sub1 main", "sub1.dmf");
		auto sd2 = root.createSubDirectory("sub2");
		(void) sd2.createSubFile("sub2 main", "sub2.dmf");

		(void) sd1.createSubFile("subsub main", "subsub.dmf");

		// Build two module trees from the same virtual directory and compare component hashes
		auto mt1 = ModuleTreeBuilder::create(root, "root_package_id11e3");
		auto mt2 = ModuleTreeBuilder::create(root, "root_package_id11e4");

		ASSERT_TRUE(
			(ModuleTree::getComponentHash(mt1->getModuleID()).elements
		     == std::vector<std::string>{ "root_package_id11e3", "root" })
		);
		ASSERT_TRUE(
			(ModuleTree::getComponentHash(mt2->getModuleID()).elements
		     == std::vector<std::string>{ "root_package_id11e4", "root" })
		);
		ASSERT_TRUE(
			ModuleTree::getComponentHash(mt1->getModuleID()).hash
			!= ModuleTree::getComponentHash(mt2->getModuleID()).hash
		);

		// Ensure submodule hashes differ from parent and from each other
		auto sub1   = mt1->getSubmodules().at(base::StrID("sub1"));
		auto sub2   = mt1->getSubmodules().at(base::StrID("sub2"));
		auto subsub = sub1->getSubmodules().at(base::StrID("subsub"));

		ASSERT_TRUE(
			(ModuleTree::getComponentHash(sub1->getModuleID()).elements
		     == std::vector<std::string>{ "root_package_id11e3", "root", "sub1" })
		);
		ASSERT_TRUE(
			(ModuleTree::getComponentHash(sub2->getModuleID()).elements
		     == std::vector<std::string>{ "root_package_id11e3", "root", "sub2" })
		);
		ASSERT_TRUE(
			(ModuleTree::getComponentHash(subsub->getModuleID()).elements
		     == std::vector<std::string>{ "root_package_id11e3", "root", "sub1", "subsub" })
		);

		ASSERT_TRUE(
			(ModuleTree::getComponentHash(sub1->getModuleID()).hash
		     != ModuleTree::getComponentHash(mt1->getModuleID()).hash)
		);
		ASSERT_TRUE(
			(ModuleTree::getComponentHash(sub1->getModuleID()).hash
		     != ModuleTree::getComponentHash(sub2->getModuleID()).hash)
		);
		ASSERT_TRUE(
			(ModuleTree::getComponentHash(subsub->getModuleID()).hash
		     != ModuleTree::getComponentHash(sub1->getModuleID()).hash)
		);

		ModuleTreeModifier::removeParent(sub1);

		ASSERT_TRUE(
			(ModuleTree::getComponentHash(subsub->getModuleID()).elements
		     == std::vector<std::string>{ "root_package_id11e3", "sub1", "subsub" })
		);
		ModuleTreeModifier::setParent(mt1, subsub);
		ASSERT_TRUE(
			(ModuleTree::getComponentHash(sub2->getModuleID()).elements
		     == std::vector<std::string>{ "root_package_id11e3", "sub1", "subsub", "root", "sub2" })
		);
		ASSERT_TRUE(
			(ModuleTree::getComponentHash(mt2->getModuleID()).elements
		     == std::vector<std::string>{ "root_package_id11e4", "root" })
		);
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/frontend/module_tree/tests/");
