#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/source_file.hpp>

#include <filesystem/file.hpp>
#include <tester/tester.hpp>

#include <algorithm>

using namespace compiler::frontend;

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
		TESTER_ADD_TEST(testMultipleSourceFiles);
		TESTER_ADD_TEST(testFileModifiedUpdatesContent);
		TESTER_ADD_TEST(testGetSourceFilesfromFile);
	}

private:
	void testSourceFileCreation() {
		// Create a temporary file for testing
		auto temp_file
			= fs::FileManager::createRandomTempFile("fn main() { println(\"Hello World\"); }");
		auto dummy_module = ModuleTreeBuilder::create()->finalize();

		// Create SourceFile
		auto source_file = SourceFile::create(temp_file, GetModuleID_Functor::make(dummy_module));

		// Test basic properties
		ASSERT_EQUAL(
			temp_file.getFilePath().native(), source_file->getFile().getFilePath().native()
		);
		ASSERT_EQUAL(GetModuleID_Functor::make(dummy_module), source_file->getModule());
		// Cleanup
		fs::FileManager::deleteFile(temp_file);
	}

	void testSourceFileProperties() {
		// Create test file with specific content
		auto test_content = "struct Point { x: i32, y: i32 }";
		auto temp_file    = fs::FileManager::createRandomTempFile(test_content);
		auto dummy_module = ModuleTreeBuilder::create()->finalize();

		auto source_file = SourceFile::create(temp_file, GetModuleID_Functor::make(dummy_module));

		// Test file properties
		assertTrue(source_file->getFile().isFile(), "Should be recognized as file");
		assertTrue(!source_file->getFile().isDirectory(), "Should not be recognized as directory");
		ASSERT_EQUAL(test_content, source_file->getFile().getContent().view().stringView());

		// Test FileID uniqueness
		auto another_source_file
			= SourceFile::create(temp_file, GetModuleID_Functor::make(dummy_module));
		assertTrue(source_file != another_source_file, "SourceFiles should be different");

		// Cleanup
		fs::FileManager::deleteFile(temp_file);
	}

	void testPSTGeneration() {
		// Create test file with valid syntax
		auto test_content = "fn test() { return 42; }";
		auto temp_file    = fs::FileManager::createRandomTempFile(test_content);
		auto dummy_module = ModuleTreeBuilder::create()->finalize();

		auto source_file = SourceFile::create(temp_file, GetModuleID_Functor::make(dummy_module));

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

		auto dummy_module = ModuleTreeBuilder::create()->finalize();

		try {
			// Test basic file content first
			auto basic_content1 = temp_file1.getContent();

			// Create SourceFile objects - this will cache the content
			auto source_file1
				= SourceFile::create(temp_file1, GetModuleID_Functor::make(dummy_module));
			auto source_file2
				= SourceFile::create(temp_file2, GetModuleID_Functor::make(dummy_module));

			// Test getCachedContent on SourceFile objects
			auto cached_content1 = source_file1->getCachedContent();
			auto cached_content2 = source_file2->getCachedContent();

			ASSERT_EQUAL(content1, cached_content1.view().stringView());
			ASSERT_EQUAL(content2, cached_content2.view().stringView());

			// Test that content is actually cached (call again)
			auto cached_content1_again = source_file1->getCachedContent();
			ASSERT_EQUAL(content1, cached_content1_again.view().stringView());

			// Test with same file path - create new SourceFile with same path
			auto same_file_source
				= SourceFile::create(temp_file1, GetModuleID_Functor::make(dummy_module));
			auto cached_content_same = same_file_source->getCachedContent();
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
		auto dummy_module = ModuleTreeBuilder::create()->finalize();

		auto source_file1 = SourceFile::create(temp_file1, GetModuleID_Functor::make(dummy_module));
		auto source_file2 = SourceFile::create(temp_file2, GetModuleID_Functor::make(dummy_module));

		// Test hash generation
		u64 hash1 = GetFileID_Functor::make(source_file1).queryUnstablePerfectHash();
		u64 hash2 = GetFileID_Functor::make(source_file2).queryUnstablePerfectHash();

		// Hashes should be different for different files
		assertTrue(hash1 != hash2, "Hashes should be different for different files");

		// Same file should return same hash
		u64 hash1_again = GetFileID_Functor::make(source_file1).queryUnstablePerfectHash();
		ASSERT_EQUAL(hash1, hash1_again);

		// Create another SourceFile with same path - the hash must be different this is because the
		// file might be in different module and the manging names will be different
		auto source_file1_copy
			= SourceFile::create(temp_file1, GetModuleID_Functor::make(dummy_module));
		u64 hash1_copy = GetFileID_Functor::make(source_file1_copy).queryUnstablePerfectHash();
		ASSERT_TRUE(hash1_copy != hash1);

		// Cleanup
		fs::FileManager::deleteFile(temp_file1);
		fs::FileManager::deleteFile(temp_file2);
	}

	void testMultipleSourceFiles() {
		// Test working with multiple source files
		std::vector<fs::File>        temp_files;
		std::vector<Ref<SourceFile>> source_files;
		auto                         dummy_module = ModuleTreeBuilder::create()->finalize();

		// Create multiple source files
		for (int i = 0; i < 5; ++i) {
			auto content
				= "fn function" + std::to_string(i) + "() { return " + std::to_string(i) + "; }";
			auto temp_file = fs::FileManager::createRandomTempFile(content);
			temp_files.push_back(temp_file);
			source_files.push_back(
				SourceFile::create(temp_file, GetModuleID_Functor::make(dummy_module))
			);
		}

		// Test that all files have unique IDs
		std::set<FileID> unique_ids;
		for (const auto& source_file: source_files) {
			auto file_id = GetFileID_Functor::make(source_file);
			assertTrue(unique_ids.find(file_id) == unique_ids.end(), "FileIDs should be unique");
			unique_ids.insert(file_id);
		}

		// Test that all files belong to the same module
		for (const auto& source_file: source_files)
			ASSERT_EQUAL(GetModuleID_Functor::make(dummy_module), source_file->getModule());

		// Test PST generation for all files
		for (auto& source_file: source_files) source_file->getPST();

		// Test unique hashes
		std::set<u64> unique_hashes;
		for (auto& source_file: source_files) {
			u64 hash = GetFileID_Functor::make(source_file).queryUnstablePerfectHash();
			assertTrue(unique_hashes.find(hash) == unique_hashes.end(), "Hashes should be unique");
			unique_hashes.insert(hash);
		}

		// Cleanup
		for (const auto& temp_file: temp_files) fs::FileManager::deleteFile(temp_file);
	}

	void testSourceFileWithDifferentModules() {
		// Test source files belonging to different modules
		auto temp_file = fs::FileManager::createRandomTempFile("fn shared_function() {}");

		auto dummy_module1 = ModuleTreeBuilder::create()->finalize();
		auto dummy_module2 = ModuleTreeBuilder::create()->finalize();

		auto source_file1 = SourceFile::create(temp_file, GetModuleID_Functor::make(dummy_module1));
		auto source_file2 = SourceFile::create(temp_file, GetModuleID_Functor::make(dummy_module2));

		// Should have different IDs even with same file path
		assertTrue(
			GetFileID_Functor::make(source_file1) != GetFileID_Functor::make(source_file2),
			"Should have different FileIDs"
		);

		// Should belong to different modules
		ASSERT_EQUAL(GetModuleID_Functor::make(dummy_module1), source_file1->getModule());
		ASSERT_EQUAL(GetModuleID_Functor::make(dummy_module2), source_file2->getModule());
		assertTrue(
			source_file1->getModule() != source_file2->getModule(),
			"Should belong to different modules"
		);

		// Should have different hashes
		u64 hash1 = GetFileID_Functor::make(source_file1).queryUnstablePerfectHash();
		u64 hash2 = GetFileID_Functor::make(source_file2).queryUnstablePerfectHash();
		assertTrue(hash1 != hash2, "Should have different hashes");

		// Cleanup
		fs::FileManager::deleteFile(temp_file);
	}

	void testFileModifiedUpdatesContent() {
		// Create a temp file and SourceFile
		auto temp_file    = fs::FileManager::createRandomVirtualFile("original content");
		auto dummy_module = ModuleTreeBuilder::create()->finalize();
		auto source_file  = SourceFile::create(temp_file, GetModuleID_Functor::make(dummy_module));

		// Check initial cached content
		ASSERT_EQUAL("original content", source_file->getCachedContent().view().stringView());

		// Modify file content
		temp_file.writeToFile("new content");
		// Call fileModified
		ModuleTreeModifier::fileModified(temp_file);

		std::cerr << "After modification, temp_file content: "
				  << temp_file.getContent().view().stringView() << '\n';
		std::cerr << "After modification, sourcefile content: "
				  << source_file->getCachedContent().view().stringView() << '\n';
		// SourceFile should have updated cached content
		ASSERT_EQUAL("new content", source_file->getCachedContent().view().stringView());

		fs::FileManager::deleteFile(temp_file);
	}

	void testGetSourceFilesfromFile() {
		auto temp_file     = fs::FileManager::createRandomTempFile("abc");
		auto dummy_module1 = ModuleTreeBuilder::create()->finalize();
		auto dummy_module2 = ModuleTreeBuilder::create()->finalize();
		// Create two SourceFiles for the same fs::File but different modules
		auto source_file1 = SourceFile::create(temp_file, GetModuleID_Functor::make(dummy_module1));
		auto source_file2 = SourceFile::create(temp_file, GetModuleID_Functor::make(dummy_module2));
		// Should both be returned by getSourceFilesfromFile
		auto files_vec = SourceFile::getSourceFilesfromFile(temp_file);
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
};

TESTER_COMMON_MAIN("/src/compiler/core/frontend/module_tree/tests/");
