#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_flags/module_flags.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/source_file.hpp>

#include <base/config/build_type.hpp>
#include <base/except/exceptions.hpp>

#include <filesystem/file.hpp>
#include <hashing/add_to_hash.hpp>
#include <tester/tester.hpp>

using namespace compiler::frontend;

namespace {
	base::Ref<SourceFile> getRef(AccessLocked<FileID> access) {
		return GetFileID_Functor::getFileRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
			access.illegalAccess().getID()
		);
	}
}

class SourceFileTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SourceFileTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testSourceFileCreation);
		TESTER_ADD_TEST(testSourceFileProperties);
		TESTER_ADD_TEST(testPSTGeneration);
		TESTER_ADD_TEST(testContentCaching);
		TESTER_ADD_TEST(testHashGeneration);
		TESTER_ADD_TEST(testComponentHashComputation);
		TESTER_ADD_TEST(testMultipleSourceFiles);
		TESTER_ADD_TEST(testFileModifiedUpdatesContent);
		TESTER_ADD_TEST(testGetSourceFilesFromFile);
		TESTER_ADD_TEST(testSourceFileRemovalClearsLookups);
		TESTER_ADD_TEST(testSourceFileDanglingReferenceDetection);
	}

protected:
	void beforeAll() override { ::compiler::frontend::use_module_modifier_remove = true; }

private:
	void testSourceFileCreation() {
		// Create a temporary file for testing
		auto temp_file
			= fs::FileManager::createRandomTempFile("fn main() { println(\"Hello World\"); }");
		auto dummy_module = ModuleTreeBuilder::createWithRandomPackageID()->finalize();

		// Create SourceFile
		auto source_file = SourceFile::create(temp_file, dummy_module->getModuleID());

		// Test basic properties
		ASSERT_EQUAL(
			temp_file.getFilePath().native(),
			source_file->getFileIllegalAccess().getFilePath().native()
		);
		ASSERT_EQUAL(dummy_module->getModuleID(), source_file->getModule().illegalAccess().getID());
		// Cleanup
		fs::FileManager::deleteFile(temp_file);
	}

	void testSourceFileProperties() {
		// Create test file with specific content
		auto test_content = "struct Point { x: i32, y: i32 }";
		auto temp_file    = fs::FileManager::createRandomTempFile(test_content);
		auto dummy_module = ModuleTreeBuilder::createWithRandomPackageID()->finalize();

		auto source_file = SourceFile::create(temp_file, dummy_module->getModuleID());

		// Test file properties
		assertTrue(source_file->getFileIllegalAccess().isFile(), "Should be recognized as file");
		assertTrue(
			!source_file->getFileIllegalAccess().isDirectory(),
			"Should not be recognized as directory"
		);
		ASSERT_EQUAL(
			test_content, source_file->getFileIllegalAccess().getContent().view().stringView()
		);

		// Test FileID uniqueness
		auto another_source_file = SourceFile::create(temp_file, dummy_module->getModuleID());
		assertTrue(source_file != another_source_file, "SourceFiles should be different");

		// Cleanup
		fs::FileManager::deleteFile(temp_file);
	}

	void testPSTGeneration() {
		// Create test file with valid syntax
		auto test_content = "fn test() { return 42; }";
		auto temp_file    = fs::FileManager::createRandomTempFile(test_content);
		auto dummy_module = ModuleTreeBuilder::createWithRandomPackageID()->finalize();

		auto source_file = SourceFile::create(temp_file, dummy_module->getModuleID());

		// Get PST - this should trigger parsing
		source_file->getPST();

		// Cleanup
		fs::FileManager::deleteFile(temp_file);
	}

	void testContentCaching() {
		// Create test files
		auto content1 = "fn function1() {}";
		auto content2 = "fn function2() {}";

		auto temp_file1 = fs::FileManager::createRandomTempFile(content1);
		auto temp_file2 = fs::FileManager::createRandomTempFile(content2);

		auto dummy_module = ModuleTreeBuilder::createWithRandomPackageID()->finalize();

		try {
			// Test basic file content first
			auto basic_content1 = temp_file1.getContent();

			// Create SourceFile objects - this will cache the content
			auto source_file1 = SourceFile::create(temp_file1, dummy_module->getModuleID());
			auto source_file2 = SourceFile::create(temp_file2, dummy_module->getModuleID());

			// Test getCachedContentIllegalAccess() on SourceFile objects
			auto cached_content1 = source_file1->getCachedContentIllegalAccess();
			auto cached_content2 = source_file2->getCachedContentIllegalAccess();

			ASSERT_EQUAL(content1, cached_content1.view().stringView());
			ASSERT_EQUAL(content2, cached_content2.view().stringView());

			// Test that content is actually cached (call again)
			auto cached_content1_again = source_file1->getCachedContentIllegalAccess();
			ASSERT_EQUAL(content1, cached_content1_again.view().stringView());

			// Test with same file path - create new SourceFile with same path
			auto same_file_source    = SourceFile::create(temp_file1, dummy_module->getModuleID());
			auto cached_content_same = same_file_source->getCachedContentIllegalAccess();
			ASSERT_EQUAL(content1, cached_content_same.view().stringView());

		} catch (const std::exception& e) {
			// Cleanup before re-throwing
			fs::FileManager::deleteFile(temp_file1);
			fs::FileManager::deleteFile(temp_file2);
			throw;
		}

		// Cleanup
		fs::FileManager::deleteFile(temp_file1);
		fs::FileManager::deleteFile(temp_file2);
	}

	void testHashGeneration() {
		// Create test files
		auto temp_file1   = fs::FileManager::createRandomTempFile("content1");
		auto temp_file2   = fs::FileManager::createRandomTempFile("content2");
		auto dummy_module = ModuleTreeBuilder::createWithRandomPackageID()->finalize();

		auto source_file1 = SourceFile::create(temp_file1, dummy_module->getModuleID());
		auto source_file2 = SourceFile::create(temp_file2, dummy_module->getModuleID());

		// Test hash generation
		u64 hash1 = source_file1->getFileID().queryUnstablePerfectHash();
		u64 hash2 = source_file2->getFileID().queryUnstablePerfectHash();

		// Hashes should be different for different files
		assertTrue(hash1 != hash2, "Hashes should be different for different files");

		// Same file should return same hash
		u64 hash1_again = source_file1->getFileID().queryUnstablePerfectHash();
		ASSERT_EQUAL(hash1, hash1_again);

		// Create another SourceFile with same path - the hash must be different this is because the
		// file might be in different module and the mangled names will be different
		auto source_file1_copy = SourceFile::create(temp_file1, dummy_module->getModuleID());
		u64  hash1_copy        = source_file1_copy->getFileID().queryUnstablePerfectHash();
		ASSERT_TRUE(hash1_copy != hash1);

		// Cleanup
		fs::FileManager::deleteFile(temp_file1);
		fs::FileManager::deleteFile(temp_file2);
	}

	void testMultipleSourceFiles() {
		// Test working with multiple source files
		std::vector<fs::File>        temp_files;
		std::vector<Ref<SourceFile>> source_files;
		auto dummy_module = ModuleTreeBuilder::createWithRandomPackageID()->finalize();

		// Create multiple source files
		for (int i = 0; i < 5; ++i) {
			auto content
				= "fn function" + std::to_string(i) + "() { return " + std::to_string(i) + "; }";
			auto temp_file = fs::FileManager::createRandomTempFile(content);
			temp_files.push_back(temp_file);
			source_files.push_back(SourceFile::create(temp_file, dummy_module->getModuleID()));
		}

		// Test that all files have unique IDs
		std::set<FileID> unique_ids;
		for (const auto& source_file: source_files) {
			auto file_id = source_file->getFileID();
			assertTrue(unique_ids.find(file_id) == unique_ids.end(), "FileIDs should be unique");
			unique_ids.insert(file_id);
		}

		// Test that all files belong to the same module
		for (const auto& source_file: source_files)
			ASSERT_EQUAL(
				dummy_module->getModuleID(), source_file->getModule().illegalAccess().getID()
			);

		// Test PST generation for all files
		for (auto& source_file: source_files) source_file->getPST();

		// Test unique hashes
		std::set<u64> unique_hashes;
		for (auto& source_file: source_files) {
			u64 hash = source_file->getFileID().queryUnstablePerfectHash();
			assertTrue(unique_hashes.find(hash) == unique_hashes.end(), "Hashes should be unique");
			unique_hashes.insert(hash);
		}

		// Cleanup
		for (const auto& temp_file: temp_files) fs::FileManager::deleteFile(temp_file);
	}

	void testSourceFileWithDifferentModules() {
		// Test source files belonging to different modules
		auto temp_file = fs::FileManager::createRandomTempFile("fn shared_function() {}");

		auto dummy_module1 = ModuleTreeBuilder::createWithRandomPackageID()->finalize();
		auto dummy_module2 = ModuleTreeBuilder::createWithRandomPackageID()->finalize();

		auto source_file1 = SourceFile::create(temp_file, dummy_module1->getModuleID());
		auto source_file2 = SourceFile::create(temp_file, dummy_module2->getModuleID());

		// Should have different IDs even with same file path
		assertTrue(
			source_file1->getFileID() != source_file2->getFileID(), "Should have different FileIDs"
		);

		// Should belong to different modules
		ASSERT_EQUAL(
			dummy_module1->getModuleID(), source_file1->getModule().illegalAccess().getID()
		);
		ASSERT_EQUAL(
			dummy_module2->getModuleID(), source_file2->getModule().illegalAccess().getID()
		);
		assertTrue(
			source_file1->getModule().illegalAccess().getID()
				!= source_file2->getModule().illegalAccess().getID(),
			"Should belong to different modules"
		);

		// Should have different hashes
		u64 hash1 = source_file1->getFileID().queryUnstablePerfectHash();
		u64 hash2 = source_file2->getFileID().queryUnstablePerfectHash();
		assertTrue(hash1 != hash2, "Should have different hashes");

		// Cleanup
		fs::FileManager::deleteFile(temp_file);
	}

	void testFileModifiedUpdatesContent() {
		// Create a temp file and SourceFile
		auto temp_file    = fs::FileManager::createRandomVirtualFile("original content");
		auto dummy_module = ModuleTreeBuilder::createWithRandomPackageID()->finalize();
		auto source_file  = SourceFile::create(temp_file, dummy_module->getModuleID());

		// Check initial cached content
		ASSERT_EQUAL(
			"original content", source_file->getCachedContentIllegalAccess().view().stringView()
		);

		// Modify file content
		temp_file.writeToFile("new content");
		// Call fileModified
		ModuleTreeModifier::fileModified(temp_file);

		std::cerr << "After modification, temp_file content: "
				  << temp_file.getContent().view().stringView() << '\n';
		std::cerr << "After modification, sourcefile content: "
				  << source_file->getCachedContentIllegalAccess().view().stringView() << '\n';
		// SourceFile should have updated cached content
		ASSERT_EQUAL(
			"new content", source_file->getCachedContentIllegalAccess().view().stringView()
		);

		fs::FileManager::deleteFile(temp_file);
	}

	void testGetSourceFilesFromFile() {
		auto temp_file     = fs::FileManager::createRandomTempFile("abc");
		auto dummy_module1 = ModuleTreeBuilder::createWithRandomPackageID()->finalize();
		auto dummy_module2 = ModuleTreeBuilder::createWithRandomPackageID()->finalize();
		// Create two SourceFiles for the same fs::File but different modules
		auto source_file1 = SourceFile::create(temp_file, dummy_module1->getModuleID());
		auto source_file2 = SourceFile::create(temp_file, dummy_module2->getModuleID());
		// Should both be returned by getSourceFilesFromFile
		auto files_vec = SourceFile::getSourceFilesFromFile(temp_file);
		assertTrue(
			std::ranges::find(files_vec, source_file1) != files_vec.end(),
			"source_file1 should be found"
		);
		assertTrue(
			std::ranges::find(files_vec, source_file2) != files_vec.end(),
			"source_file2 should be found"
		);
		fs::FileManager::deleteFile(temp_file);
	}

	void testSourceFileRemovalClearsLookups() {
		std::vector<fs::File> cleanup_files;
		auto                  module_builder = ModuleTreeBuilder::create();
		module_builder->setName(base::StrID("sf_removal_mod"));
		module_builder->setPackageID(base::StrID("sf_removal_pkg"));
		auto main_file = fs::FileManager::createRandomVirtualFile("fn main() {}");
		cleanup_files.push_back(main_file);
		module_builder->setMainSourceFile(main_file);
		auto module = module_builder->finalize();

		auto before = SourceFile::getSourceFilesFromFile(main_file);
		ASSERT_EQUAL(1, before.size());

		ModuleTreeModifier::removeModuleRecursive(module);
		auto after = SourceFile::getSourceFilesFromFile(main_file);
		ASSERT_TRUE(after.empty());

		for (auto& file: cleanup_files) fs::FileManager::deleteFile(file);
	}

	void testSourceFileDanglingReferenceDetection() {
		std::vector<fs::File> cleanup_files;
		auto                  module_builder = ModuleTreeBuilder::create();
		module_builder->setName(base::StrID("sf_dangling_mod"));
		module_builder->setPackageID(base::StrID("sf_dangling_pkg"));
		auto main_file = fs::FileManager::createRandomVirtualFile("fn main() {}");
		cleanup_files.push_back(main_file);
		module_builder->setMainSourceFile(main_file);

		auto module = module_builder->finalize();

		auto file_id = module->getMainSourceFile().illegalAccess();

		ModuleTreeModifier::removeModuleRecursive(module);

		IF_BUILD_TYPE_DEV(assertThrows<base::Panic>(
							  [&]() { (void) GetFileID_Functor::get(file_id.getID()); },
							  "Dangling SourceFile should panic after removal"
		);)

		for (auto& file: cleanup_files) fs::FileManager::deleteFile(file);
	}

	void testComponentHashComputation() {
		// Prepare virtual directory with named files so stems are controlled
		auto root       = fs::FileManager::createRandomVirtualDirectory();
		auto file_same  = root.createSubFile("content", "same.duck");
		auto file_other = root.createSubFile("content", "other.duck");

		// Build first module (named "modA")
		auto mod_a_builder = ModuleTreeBuilder::createWithRandomPackageID();
		mod_a_builder->setName(base::StrID("modA"));
		auto mod_a_main = root.createSubFile("mainA", "modA.dmf");
		mod_a_builder->setMainSourceFile(mod_a_main);
		auto mod_a = mod_a_builder->finalize();

		// Build second module (named "modB")
		auto mod_b_builder = ModuleTreeBuilder::createWithRandomPackageID();
		mod_b_builder->setName(base::StrID("modB"));
		auto mod_b_main = root.createSubFile("mainB", "modB.dmf");
		mod_b_builder->setMainSourceFile(mod_b_main);
		auto mod_b = mod_b_builder->finalize();

		// Create SourceFile instances for the same fs::File inside the same module
		auto sf_1 = SourceFile::create(file_same, mod_a->getModuleID());
		auto sf_2 = SourceFile::create(file_same, mod_a->getModuleID());

		// Compute finalized path-hash for each SourceFile by starting from module partial
		// and adding the language-level file name. Do NOT construct ComponentHash manually here.
		{
			// start from module partial hasher
			auto parent_partial = ModuleTree::getPathComponentHash(mod_a->getModuleID()).partial;
			auto hasher1        = parent_partial;
			hashing::addToHash(hasher1, sf_1->getLangFileName());
			auto final_a_1 = hasher1.finalize();

			auto hasher2 = parent_partial;
			hashing::addToHash(hasher2, sf_2->getLangFileName());
			auto final_a_2 = hasher2.finalize();

			// Same file stem in same module -> hashes must match
			ASSERT_EQUAL(final_a_1, final_a_2);

			// Different filename in same module -> different hash
			auto sf_other     = SourceFile::create(file_other, mod_a->getModuleID());
			auto hasher_other = parent_partial;
			hashing::addToHash(hasher_other, sf_other->getLangFileName());
			auto final_a_other = hasher_other.finalize();
			ASSERT_TRUE(final_a_1 != final_a_other);

			// Same file stem but different module -> different hash
			auto parent_b_partial = ModuleTree::getPathComponentHash(mod_b->getModuleID()).partial;
			auto hasher_b         = parent_b_partial;
			hashing::addToHash(hasher_b, sf_1->getLangFileName());
			auto final_b_same = hasher_b.finalize();
			ASSERT_TRUE(final_a_1 != final_b_same);
		}

		// Cleanup virtual files
		fs::FileManager::deleteFile(file_same);
		fs::FileManager::deleteFile(file_other);
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/frontend/module_tree/tests/");
