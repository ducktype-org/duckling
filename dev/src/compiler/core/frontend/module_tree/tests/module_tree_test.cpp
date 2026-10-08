// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_flags/module_flags.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/module_tree_builder.hpp>
#include <frontend/module_tree/module_tree_modifier.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/packages/access.hpp>

#include <base/config/build_type.hpp>
#include <base/except/exceptions.hpp>

#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

using namespace compiler::frontend;

namespace {
	auto getRef(AccessLocked<ModuleID> access) {
		return GetModuleID_Functor::getModRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
			access.illegalAccess().getID()
		);
	}

	auto getRef(AccessLocked<FileID> access) {
		return GetFileID_Functor::getFileRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
			access.illegalAccess().getID()
		);
	}

	// Helper to check if a submodule exists by name
	bool hasSubmodule(const std::vector<ModuleAccessLocked>& submodules, base::StrID name) {
		for (const auto& submod: submodules)
			if (getRef(submod)->getName() == name) return true;
		return false;
	}

	// Helper to get a submodule by name
	ModuleAccessLocked getSubmodule(
		const std::vector<ModuleAccessLocked>& submodules, base::StrID name
	) {
		for (const auto& submod: submodules)
			if (getRef(submod)->getName() == name) return submod;
		throw std::out_of_range("Submodule not found");
	}

	ModuleAccessLocked getSubmoduleIllegal(
		const SubmodulesAccessLocked& submodules, base::StrID name
	) {
		return getSubmodule(submodules.illegalAccess(), name);
	}
}

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
		TESTER_ADD_TEST(testModuleRemovalOperations);
		TESTER_ADD_TEST(testModuleRecursiveRemoval);
		TESTER_ADD_TEST(testComponentHash);
		TESTER_ADD_TEST(testPrintModuleTree);
		TESTER_ADD_TEST(testFileResolverSubstitution);
	}

protected:
	void beforeAll() override { ::compiler::frontend::use_module_modifier_remove = true; }

private:
	void testPrintModuleTree() {
		// Test with valid name
		auto temp_file            = fs::FileManager::createRandomVirtualFile("fn main() {}\n");
		auto dummy_module_builder = ModuleTreeBuilder::create();
		dummy_module_builder->setName(base::StrID("dummy"));
		dummy_module_builder->setPackageID(base::StrID("dummy_package"));
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
		no_name_builder->setPackageID(base::StrID("no_name_package"));
		// do NOT set name
		no_name_builder->setMainSourceFile(temp_file2);
		auto        no_name_module = no_name_builder->finalize();
		auto        no_name_id     = no_name_module->getModuleID();
		auto        tree_str2      = printModuleTree(no_name_id);
		std::string hash_str       = no_name_id.queryUnstablePerfectHash().toStringHex();
		assertTrue(
			tree_str2.find("id:") != std::string::npos, "Output should contain 'id:' for bad name"
		);
		assertTrue(
			tree_str2.find(hash_str) != std::string::npos, "Output should contain module hash"
		);
		fs::FileManager::deleteFile(temp_file2);
	}

	void testFileResolverSubstitution() {
		auto root = fs::File(path("test_module"));

		// Substitute a buffer for awe.dk, leaving every other file on disk untouched. The twin
		// keeps the file name, exactly as the editor overlay does.
		auto substitute = fs::FileManager::createVirtualFile(
			fs::FilePath(path("test_module/awe.dk")).toVirtualPath(), "fn substituted() {}\n", true
		);

		std::vector<std::string> resolved;
		auto                     resolver = [&](const fs::File& disk_file) -> fs::File {
            resolved.push_back(disk_file.name());
            if (disk_file.name() == "awe.dk") return substitute;
            return disk_file;
		};

		auto module_id
			= ModuleTreeBuilder::create(
				  root, base::StrID("resolver_package_id"), resolver, test_regex, test_regex
			)
		          ->getModuleID();
		auto mt = getModuleRef(module_id);

		assertTrue(
			std::ranges::find(resolved, std::string("awe.dk")) != resolved.end(),
			"The resolver must see every file the walk finds"
		);

		ASSERT_TRUE(mt->hasMainSourceFile());
		ASSERT_EQUAL(2, mt->getSubmodules().illegalAccess().size());

		auto awe = getSubmodule(mt->getSubmodules().illegalAccess(), base::StrID("awe"));
		ASSERT_TRUE(getRef(awe)->hasMainSourceFile());
		assertTrue(
			getFileRef(getRef(awe)->getMainSourceFile().illegalAccess().getID())
					->getFileIllegalAccess()
					.getFilePath()
				== substitute.getFilePath(),
			"The substituted file must back the module, not the one on disk"
		);

		auto another = getSubmodule(mt->getSubmodules().illegalAccess(), base::StrID("another"));
		assertTrue(
			getFileRef(getRef(another)->getMainSourceFile().illegalAccess().getID())
				->getFileIllegalAccess()
				.getFilePath()
				.isPhysical(),
			"Files the resolver passed through must still come from disk"
		);

		fs::FileManager::deleteFile(substitute);
	}

	void parseModule() {
		auto pth = fs::File(path("test_module"));
		auto mt  = ModuleTreeBuilder::create(
            pth, base::StrID("test_package_id"), identityFileResolver(), test_regex, test_regex
        );

		ASSERT_TRUE(mt->hasMainSourceFile());
		ASSERT_EQUAL(2, mt->getSubmodules().illegalAccess().size());
		ASSERT_EQUAL(1, mt->getOtherFiles().size());

		auto another_module
			= getSubmodule(mt->getSubmodules().illegalAccess(), base::StrID("another"));
		ASSERT_TRUE(getRef(another_module)->hasMainSourceFile());
		ASSERT_EQUAL(
			2, getRef(another_module)->getOtherFiles().size()
		);  // 2, because there are 2 different file extensions
		ASSERT_EQUAL(2, getRef(another_module)->getOtherFiles()[base::StrID(".txt")].size());
		ASSERT_EQUAL(1, getRef(another_module)->getOtherFiles()[base::StrID("")].size());
		ASSERT_EQUAL(1, getRef(another_module)->getSubmodules().illegalAccess().size());


		ASSERT_TRUE(hasSubmodule(mt->getSubmodules().illegalAccess(), base::StrID("awe")));
		auto awe_module = getSubmodule(mt->getSubmodules().illegalAccess(), base::StrID("awe"));
		ASSERT_EQUAL(0, getRef(awe_module)->getSubmodules().illegalAccess().size());
		ASSERT_EQUAL(0, getRef(awe_module)->getOtherFiles().size());
		ASSERT_TRUE(getRef(awe_module)->hasMainSourceFile());
		ASSERT_EQUAL(
			"awe.dk", getRef(getRef(awe_module)->getMainSourceFile())->getFileIllegalAccess().name()
		);
	}

	void testModuleIDInSourceFile(base::CRef<ModuleTree> module) {
		std::cerr << "Testing module ID in source files for module: " << module->getName().strView()
				  << '\n';
		auto id = module->getModuleID();
		std::cerr << "Module id: " << id.queryUnstablePerfectHash() << '\n';
		std::cerr << "Main source file Module ID: "
				  << getRef(module->getMainSourceFile())
						 ->getModule()
						 .illegalAccess()
						 .getID()
						 .queryUnstablePerfectHash()
				  << '\n';
		ASSERT_EQUAL(
			id.queryUnstablePerfectHash(),
			getRef(module->getMainSourceFile())
				->getModule()
				.illegalAccess()
				.getID()
				.queryUnstablePerfectHash()
		);
	}

	void testOtherFeatures() {
		auto pth = fs::File(path("test_module"));
		auto mt  = ModuleTreeBuilder::create(pth, base::StrID("another_package_id"));

		ASSERT_EQUAL("test_module", mt->getName());
		ASSERT_TRUE(mt->hasMainSourceFile());
		ASSERT_EQUAL(
			"content123\n",
			getRef(mt->getMainSourceFile())->getFileIllegalAccess().getContent().view()
		);
		ASSERT_TRUE(mt->getParentModule().empty());
		ASSERT_EQUAL(
			mt->getName(),
			getRef(getRef(getSubmodule(mt->getSubmodules().illegalAccess(), base::StrID("awe")))
		               ->getParentModule()
		               .value())
				->getName()
		);

		auto awe_module = getSubmodule(mt->getSubmodules().illegalAccess(), base::StrID("awe"));
		auto another_module
			= getSubmodule(mt->getSubmodules().illegalAccess(), base::StrID("another"));
		auto awesome_module = getSubmodule(
			getRef(another_module)->getSubmodules().illegalAccess(), base::StrID("awesome_module")
		);
		auto mod_module = getSubmodule(
			getRef(awesome_module)->getSubmodules().illegalAccess(), base::StrID("mod")
		);


		testModuleIDInSourceFile(getRef(awe_module));
		std::cerr << mt->prettyPrint() << '\n';
		testModuleIDInSourceFile(getRef(another_module));
		testModuleIDInSourceFile(getRef(awesome_module));
		testModuleIDInSourceFile(getRef(mod_module));
		testModuleIDInSourceFile(mt);

		ASSERT_NO_VALUE(mt->getParentModule(), "Root module has a parent");
		ASSERT_HAS_VALUE(
			getRef(awe_module)->getParentModule(), "Non-root module does not have a parent (1)"
		);
		ASSERT_HAS_VALUE(
			getRef(another_module)->getParentModule(), "Non-root module does not have a parent (2)"
		);
		ASSERT_HAS_VALUE(
			getRef(awesome_module)->getParentModule(), "Non-root module does not have a parent (3)"
		);
		ASSERT_HAS_VALUE(
			getRef(mod_module)->getParentModule(), "Non-root module does not have a parent (4)"
		);

		ASSERT_EQUAL(
			mt->getModuleID(), getRef(getRef(awe_module)->getParentModule().value())->getModuleID()
		);
		ASSERT_EQUAL(
			mt->getModuleID(),
			getRef(getRef(another_module)->getParentModule().value())->getModuleID()
		);
		ASSERT_EQUAL(
			getRef(another_module)->getModuleID(),
			getRef(getRef(awesome_module)->getParentModule().value())->getModuleID()
		);
		ASSERT_EQUAL(
			getRef(awesome_module)->getModuleID(),
			getRef(getRef(mod_module)->getParentModule().value())->getModuleID()
		);
	}

	void testQueries() {
		auto pth  = fs::File(path("test_module"));
		auto root = compiler::frontend::createModuleTreeWithRandomPackageID(pth);

		[[maybe_unused]] auto awe
			= query::entryPoint<QuerySubmodules>(root)->at(base::StrID("awe"));

		auto main_id = query::entryPoint<QueryMainSourceFile>(root);

		query::utils::withContextDo([&](query::Context& ctx) { getFilePST(ctx, main_id); });
	}

	void testParseDirectoryLikeFsTree() {
		// This test is adapted from the old FsTree parseDirectory test.
		const auto root = fs::File(path("test_directory_tree"));
		auto       mt   = ModuleTreeBuilder::create(
            root,
            base::StrID("test_package_id23423423"),
            identityFileResolver(),
            test_regex,
            test_regex
        );

		// Only files with valid names/extensions are included as source or other files.
		// Check that only the correct files and directories are present as submodules or files.
		// Since ModuleTree does not expose raw file names, we check via submodules and files.

		// Check submodules (directories)
		ASSERT_TRUE(
			hasSubmodule(mt->getSubmodules().illegalAccess(), base::StrID("another_directory"))
		);
		ASSERT_EQUAL(
			false,
			hasSubmodule(mt->getSubmodules().illegalAccess(), base::StrID(".skipped_directory"))
		);

		// Check files (source/other)
		bool found_file = false, found_file_txt = false, found_skipped = false;
		for (const auto& [ext, files]: mt->getOtherFiles()) {
			for (const auto& file: files) {
				if (file.name() == "file") found_file = true;
				if (file.name() == "file.txt") found_file_txt = true;
				if (file.name() == ".skipped_file") found_skipped = true;
			}
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
		auto another_dir
			= getSubmodule(mt->getSubmodules().illegalAccess(), base::StrID("another_directory"));
		ASSERT_EQUAL(0, getRef(another_dir)->getOtherFiles().size());
	}

	void testVirtualFilesLikeModuleTree() {
		// This test is adapted from the old FsTree virtual files test.
		// Create a virtual root directory
		auto root = fs::FileManager::createRandomVirtualDirectory();

		// Create subdirectories and files
		auto sub_dir1 = root.createSubDirectory("subDir1");
		auto sub_dir2 = root.createSubDirectory("subDir2");
		auto file1    = root.createSubFile("File1 content", "file1.txt");
		auto file4    = sub_dir1.createSubFile("File2 content", "subDir1.dk");
		auto file2    = sub_dir1.createSubFile("File2 content", "file2.txt");
		auto file3    = sub_dir2.createSubFile("File2 content", "subDir2.dk");

		// Create ModuleTree from the virtual root directory
		auto mt = ModuleTreeBuilder::create(root, base::StrID("virtual_package_id1312"));

		// Test root module name
		ASSERT_EQUAL(root.name(), mt->getName().strView());

		// Test submodules (directories)
		ASSERT_TRUE(hasSubmodule(mt->getSubmodules().illegalAccess(), base::StrID("subDir1")));
		ASSERT_TRUE(hasSubmodule(mt->getSubmodules().illegalAccess(), base::StrID("subDir2")));

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
		ASSERT_TRUE(found_file1);

		// Test files in subDir1
		auto sub1 = getSubmodule(mt->getSubmodules().illegalAccess(), base::StrID("subDir1"));
		bool found_file2 = false;
		for (const auto& [ext, files]: getRef(sub1)->getOtherFiles()) {
			for (const auto& file: files) {
				if (file.name() == "file2.txt") {
					ASSERT_EQUAL("File2 content", file.getContent().view());
					found_file2 = true;
				}
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
		auto file1    = root_dir.createSubFile("main content", "main.dk");
		auto file2    = root_dir.createSubFile("src content", "src1.duck");
		auto file3    = root_dir.createSubFile("other content", "other.txt");
		auto file4    = root_dir.createSubFile("other2 content", "other2.md");

		// Build initial module tree
		auto mt = ModuleTreeBuilder::create(root_dir, base::StrID("modifier_test_package_id65"));
		// Test addSourceFile


		// Test setMainSourceFile (remove first if exists)
		if (mt->hasMainSourceFile()) {
			ModuleTreeModifier::removeMainSourceFile(mt);
			ASSERT_EQUAL(false, mt->hasMainSourceFile());
		}
		auto new_main = root_dir.createSubFile("main2", "main2.dk");
		ModuleTreeModifier::setMainSourceFile(mt, new_main);
		ASSERT_EQUAL("main2.dk", getRef(mt->getMainSourceFile())->getFileIllegalAccess().name());

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
		auto sub_mod
			= ModuleTreeBuilder::create(sub_dir, base::StrID("modifier_test_package_id65"));
		ModuleTreeModifier::addSubmodule(mt, sub_mod);
		ASSERT_TRUE(hasSubmodule(mt->getSubmodules().illegalAccess(), sub_mod->getName()));
		// Remove parent
		ModuleTreeModifier::removeParent(sub_mod);
		ASSERT_NO_VALUE(sub_mod->getParentModule());
		// Set parent again
		ModuleTreeModifier::setParent(sub_mod, mt);
		ASSERT_EQUAL(mt, getRef(sub_mod->getParentModule().value()));

		// Test removeModule
		ModuleTreeModifier::removeSingleModule(sub_mod);
		ASSERT_EQUAL(
			false, hasSubmodule(mt->getSubmodules().illegalAccess(), base::StrID("submod"))
		);

		// Test fileModified (should not throw)
		// auto src_file
		// 	= mt->getSourceFiles().empty() ? new_src : mt->getSourceFiles().front();
		// ModuleTreeModifier::fileModified(src_file);

		// Test removeModule with a module that has main source file
		{
			// Use unique variable names to avoid shadowing
			auto removable_sub_dir   = root_dir.createSubDirectory("removable");
			auto removable_main_file = removable_sub_dir.createSubFile("main", "removable.dk");
			auto removable_sub_mod   = ModuleTreeBuilder::create(
                removable_sub_dir, base::StrID("modifier_test_package_id65")
            );

			// Add as submodule
			ModuleTreeModifier::addSubmodule(mt, removable_sub_mod);
			ASSERT_TRUE(
				hasSubmodule(mt->getSubmodules().illegalAccess(), removable_sub_mod->getName())
			);

			// Check that main file exists in SourceFile::file_map
			ASSERT_TRUE(removable_sub_mod->hasMainSourceFile());
			auto removable_main_id = removable_sub_mod->getMainSourceFile();
			ASSERT_TRUE(
				getRef(removable_main_id)->getModule().illegalAccess().getID()
				== removable_sub_mod->getModuleID()
			);

			// Remove the submodule
			ModuleTreeModifier::removeSingleModule(removable_sub_mod);
			ASSERT_EQUAL(
				false, hasSubmodule(mt->getSubmodules().illegalAccess(), base::StrID("removable"))
			);
		}

		{
			// Test changePackageID
			root_dir      = fs::FileManager::createRandomVirtualDirectory();
			auto root_mod = ModuleTreeBuilder::create(root_dir, base::StrID("original_package_id"));
			sub_dir       = root_dir.createSubDirectory("submod");
			sub_mod       = ModuleTreeBuilder::create(sub_dir, base::StrID("original_package_id"));
			ModuleTreeModifier::addSubmodule(root_mod, sub_mod);

			// Attempt to change package ID for non-root module (should throw)
			bool exception_thrown = false;
			try {
				ModuleTreeModifier::changePackageID(sub_mod, base::StrID("new_package_id"));
			} catch (const std::exception&) { exception_thrown = true; }
			ASSERT_TRUE(exception_thrown);

			// std::cout << root_mod->getParentModule().value()->getName().strView() << '\n';
			//  Change package ID for root module
			ModuleTreeModifier::changePackageID(root_mod, base::StrID("new_package_id"));
			ASSERT_EQUAL("new_package_id", root_mod->getPackage().illegalAccess().getID().strView());
			ASSERT_EQUAL("new_package_id", sub_mod->getPackage().illegalAccess().getID().strView());
		}
	}

	void testModuleRemovalOperations() {
		std::vector<fs::File> cleanup_files;

		auto root_builder = ModuleTreeBuilder::create();
		root_builder->setName(base::StrID("removal_root"));
		root_builder->setPackageID(base::StrID("removal_pkg"));
		auto root_main = fs::FileManager::createRandomVirtualFile("fn root() {}");
		cleanup_files.push_back(root_main);
		root_builder->setMainSourceFile(root_main);
		auto root = root_builder->finalize();

		auto child_builder = ModuleTreeBuilder::create();
		child_builder->setName(base::StrID("removal_child"));
		child_builder->setPackageID(base::StrID("removal_pkg"));
		auto child_main = fs::FileManager::createRandomVirtualFile("fn child() {}");
		cleanup_files.push_back(child_main);
		child_builder->setMainSourceFile(child_main);
		auto child = child_builder->finalize();

		auto grand_child_builder = ModuleTreeBuilder::create();
		grand_child_builder->setName(base::StrID("removal_grand"));
		grand_child_builder->setPackageID(base::StrID("removal_pkg"));
		auto grand_main = fs::FileManager::createRandomVirtualFile("fn grand() {}");
		cleanup_files.push_back(grand_main);
		grand_child_builder->setMainSourceFile(grand_main);
		auto grand_child = grand_child_builder->finalize();

		ModuleTreeModifier::addSubmodule(child, grand_child);
		ModuleTreeModifier::addSubmodule(root, child);

		ASSERT_TRUE(hasSubmodule(root->getSubmodules().illegalAccess(), base::StrID("removal_child"))
		);
		auto child_id       = child->getModuleID();
		auto grand_child_id = grand_child->getModuleID();

		ModuleTreeModifier::removeSingleModule(child);

		ASSERT_TRUE(hasSubmodule(root->getSubmodules().illegalAccess(), base::StrID("removal_grand"))
		);
		ASSERT_EQUAL(
			false, hasSubmodule(root->getSubmodules().illegalAccess(), base::StrID("removal_child"))
		);
		auto promoted
			= getSubmodule(root->getSubmodules().illegalAccess(), base::StrID("removal_grand"));
		ASSERT_HAS_VALUE(getRef(promoted)->getParentModule());
		ASSERT_EQUAL(
			root->getModuleID(), getRef(getRef(promoted)->getParentModule().value())->getModuleID()
		);

		IF_BUILD_TYPE_DEV(assertThrows<base::Panic>(
							  [&]() { std::ignore = GetModuleID_Functor::get(child_id); },
							  "Dangling ModuleTree should panic after removeModule"
		);)

		auto grand_ref = GetModuleID_Functor::get(grand_child_id);
		ASSERT_EQUAL(base::StrID("removal_grand"), grand_ref->getName());

		for (auto& file: cleanup_files) fs::FileManager::deleteFile(file);
	}

	void testModuleRecursiveRemoval() {
		std::vector<fs::File> cleanup_files;

		auto root_builder = ModuleTreeBuilder::create();
		root_builder->setName(base::StrID("recursive_root"));
		root_builder->setPackageID(base::StrID("recursive_pkg"));
		auto root_main = fs::FileManager::createRandomVirtualFile("fn root() {}");
		cleanup_files.push_back(root_main);
		root_builder->setMainSourceFile(root_main);
		auto root = root_builder->finalize();

		auto child_builder = ModuleTreeBuilder::create();
		child_builder->setName(base::StrID("recursive_child"));
		child_builder->setPackageID(base::StrID("recursive_pkg"));
		auto child_main = fs::FileManager::createRandomVirtualFile("fn child() {}");
		cleanup_files.push_back(child_main);
		child_builder->setMainSourceFile(child_main);
		auto child = child_builder->finalize();

		auto grand_child_builder = ModuleTreeBuilder::create();
		grand_child_builder->setName(base::StrID("recursive_grand"));
		grand_child_builder->setPackageID(base::StrID("recursive_pkg"));
		auto grand_main = fs::FileManager::createRandomVirtualFile("fn grand() {}");
		cleanup_files.push_back(grand_main);
		grand_child_builder->setMainSourceFile(grand_main);
		auto grand_child = grand_child_builder->finalize();

		ModuleTreeModifier::addSubmodule(child, grand_child);
		ModuleTreeModifier::addSubmodule(root, child);

		auto child_id       = child->getModuleID();
		auto grand_child_id = grand_child->getModuleID();

		ModuleTreeModifier::removeModuleRecursive(child);

		ASSERT_EQUAL(
			false,
			hasSubmodule(root->getSubmodules().illegalAccess(), base::StrID("recursive_child"))
		);
		ASSERT_TRUE(root->getSubmodules().illegalAccess().empty());

		IF_BUILD_TYPE_DEV(assertThrows<base::Panic>(
							  [&]() { std::ignore = GetModuleID_Functor::get(child_id); },
							  "Dangling ModuleTree should panic after removeModuleRecursive"
		);
		                  assertThrows<base::Panic>(
							  [&]() { std::ignore = GetModuleID_Functor::get(grand_child_id); },
							  "Recursive removal should also invalidate grandchildren"
						  );)

		for (auto& file: cleanup_files) fs::FileManager::deleteFile(file);
	}

	void testManualModuleTreeBuilder() {
		// Manually build a module tree using ModuleTreeBuilder (not from filesystem)
		auto builder = ModuleTreeBuilder::create();

		// Set name
		builder->setName(base::StrID("manual_mod"));
		builder->setPackageID(base::StrID("manual_package_id456"));

		// Create virtual files
		auto root_dir   = fs::FileManager::createRandomVirtualDirectory();
		auto main_file  = root_dir.createSubFile("main", "manual_mod.dk");
		auto other_file = root_dir.createSubFile("other", "other.txt");

		// Set main source file
		builder->setMainSourceFile(main_file);
		// Add other file
		builder->addOtherFile(other_file);

		// Add submodule
		auto sub_dir     = root_dir.createSubDirectory("subdir");
		auto sub_builder = ModuleTreeBuilder::create();
		sub_builder->setPackageID(base::StrID("manual_package_id456"));
		sub_builder->setName(base::StrID("subdir"));
		auto sub_main = sub_dir.createSubFile("submain", "subdir.dk");
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
		parent_builder->setPackageID(base::StrID("manual_package_id456"));
		auto parent_file = root_dir.createSubFile("parent", "parent_mod.dk");
		parent_builder->setMainSourceFile(parent_file);
		auto parent_mod = parent_builder->finalize();
		builder->setParent(parent_mod);

		// Finalize
		auto mt = builder->finalize();
		ASSERT_EQUAL(false, !builder->isFinalized());

		// Check structure
		ASSERT_EQUAL("manual_mod", mt->getName().strView());
		ASSERT_TRUE(mt->hasMainSourceFile());
		ASSERT_TRUE(hasSubmodule(parent_mod->getSubmodules().illegalAccess(), mt->getName()));
		ASSERT_EQUAL(1, mt->getOtherFiles().size());
		ASSERT_EQUAL(1, mt->getSubmodules().illegalAccess().size());
		ASSERT_EQUAL(
			"subdir",
			getRef(getSubmodule(mt->getSubmodules().illegalAccess(), base::StrID("subdir")))
				->getName()
				.strView()
		);
		ASSERT_TRUE(getRef(getSubmodule(mt->getSubmodules().illegalAccess(), base::StrID("subdir")))
		                ->hasMainSourceFile());
		// Check parent
		ASSERT_HAS_VALUE(mt->getParentModule());
		ASSERT_EQUAL("parent_mod", getRef(mt->getParentModule().value())->getName().strView());
	}

	void testModuleLoop() {
		auto root   = fs::FileManager::createRandomVirtualDirectory();
		auto md1    = root.createSubDirectory("md1");
		std::ignore = md1.createSubFile("main1", "md1.dk");
		auto md2    = root.createSubDirectory("md2");
		std::ignore = md2.createSubFile("main2", "md2.dk");

		auto mt1 = ModuleTreeBuilder::create(md1, base::StrID("md1_package_id"));
		auto mt2 = ModuleTreeBuilder::create(md2, base::StrID("md1_package_id"));

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
		std::ignore = root.createSubFile("root", "root.dk");

		auto sd1    = root.createSubDirectory("sub1");
		std::ignore = sd1.createSubFile("sub1 main", "sub1.dk");
		auto sd2    = root.createSubDirectory("sub2");
		std::ignore = sd2.createSubFile("sub2 main", "sub2.dk");

		std::ignore = sd1.createSubFile("subsub main", "subsub.dk");

		// Build two module trees from the same virtual directory and compare component hashes
		auto mt1 = ModuleTreeBuilder::create(root, base::StrID("root_package_id11e3"));
		auto mt2 = ModuleTreeBuilder::create(root, base::StrID("root_package_id11e4"));

#if defined(BUILD_TYPE_DEV)
		ASSERT_TRUE(
			(ModuleTree::getPathComponentHash(mt1->getModuleID()).elements
		     == std::vector<std::string>{ "root_package_id11e3", "root" })
		);
		ASSERT_TRUE(
			(ModuleTree::getPathComponentHash(mt2->getModuleID()).elements
		     == std::vector<std::string>{ "root_package_id11e4", "root" })
		);
#endif
		ASSERT_TRUE(
			ModuleTree::getPathComponentHash(mt1->getModuleID()).hash
			!= ModuleTree::getPathComponentHash(mt2->getModuleID()).hash
		);

		// Ensure submodule hashes differ from parent and from each other
		auto sub1   = getSubmodule(mt1->getSubmodules().illegalAccess(), base::StrID("sub1"));
		auto sub2   = getSubmodule(mt1->getSubmodules().illegalAccess(), base::StrID("sub2"));
		auto subsub = getSubmoduleIllegal(getRef(sub1)->getSubmodules(), base::StrID("subsub"));

#if defined(BUILD_TYPE_DEV)
		ASSERT_TRUE(
			(ModuleTree::getPathComponentHash(getRef(sub1)->getModuleID()).elements
		     == std::vector<std::string>{ "root_package_id11e3", "root", "sub1" })
		);
		ASSERT_TRUE(
			(ModuleTree::getPathComponentHash(getRef(sub2)->getModuleID()).elements
		     == std::vector<std::string>{ "root_package_id11e3", "root", "sub2" })
		);
		ASSERT_TRUE(
			(ModuleTree::getPathComponentHash(getRef(subsub)->getModuleID()).elements
		     == std::vector<std::string>{ "root_package_id11e3", "root", "sub1", "subsub" })
		);
#endif

		ASSERT_TRUE(
			(ModuleTree::getPathComponentHash(getRef(sub1)->getModuleID()).hash
		     != ModuleTree::getPathComponentHash(mt1->getModuleID()).hash)
		);
		ASSERT_TRUE(
			(ModuleTree::getPathComponentHash(getRef(sub1)->getModuleID()).hash
		     != ModuleTree::getPathComponentHash(getRef(sub2)->getModuleID()).hash)
		);
		ASSERT_TRUE(
			(ModuleTree::getPathComponentHash(getRef(subsub)->getModuleID()).hash
		     != ModuleTree::getPathComponentHash(getRef(sub1)->getModuleID()).hash)
		);

		ModuleTreeModifier::removeParent(getRef(sub1));

		IF_BUILD_TYPE_DEV({
			ASSERT_TRUE(
				(ModuleTree::getPathComponentHash(getRef(subsub)->getModuleID()).elements
			     == std::vector<std::string>{ "root_package_id11e3", "sub1", "subsub" })
			);
			ModuleTreeModifier::setParent(mt1, getRef(subsub));
			ASSERT_TRUE(
				(ModuleTree::getPathComponentHash(getRef(sub2)->getModuleID()).elements
			     == std::vector<std::string>{ "root_package_id11e3",
			                                  "sub1",
			                                  "subsub",
			                                  "root",
			                                  "sub2" })
			);
			ASSERT_TRUE(
				(ModuleTree::getPathComponentHash(mt2->getModuleID()).elements
			     == std::vector<std::string>{ "root_package_id11e4", "root" })
			);
		});
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/frontend/module_tree/tests/");
