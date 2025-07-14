
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/source_file.hpp>

#include <filesystem/file.hpp>
#include <tester/tester.hpp>

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
	}

private:
	void testSourceFileCreation() {
		// Create a temporary file for testing
		auto temp_file
			= fs::FileManager::createRandomTempFile("fn main() { println(\"Hello World\"); }");
		ModuleID test_module_id = ModuleID::next();

		// Create SourceFile
		SourceFile source_file(temp_file, test_module_id);

		// Test basic properties
		ASSERT_EQUAL(temp_file.nativePath(), source_file.path.nativePath());
		ASSERT_EQUAL(test_module_id, source_file.linked_module);
		assertTrue(source_file.id.isGood(), "FileID should be valid");
		assertTrue(!source_file.parse_tree.has_value(), "Parse tree should be initially empty");

		// Cleanup
		fs::FileManager::deleteFile(temp_file);
	}

	void testSourceFileProperties() {
		// Create test file with specific content
		auto     test_content = "struct Point { x: i32, y: i32 }";
		auto     temp_file    = fs::FileManager::createRandomTempFile(test_content);
		ModuleID module_id    = ModuleID::next();

		SourceFile source_file(temp_file, module_id);

		// Test file properties
		assertTrue(source_file.path.isFile(), "Should be recognized as file");
		assertTrue(!source_file.path.isDirectory(), "Should not be recognized as directory");
		ASSERT_EQUAL(test_content, source_file.path.getContent().view().stringView());

		// Test FileID uniqueness
		SourceFile another_source_file(temp_file, module_id);
		assertTrue(source_file.id != another_source_file.id, "FileIDs should be unique");

		// Cleanup
		fs::FileManager::deleteFile(temp_file);
	}

	void testPSTGeneration() {
		// Create test file with valid syntax
		auto     test_content = "fn test() { return 42; }";
		auto     temp_file    = fs::FileManager::createRandomTempFile(test_content);
		ModuleID module_id    = ModuleID::next();

		SourceFile source_file(temp_file, module_id);

		// Initially parse tree should be empty
		assertTrue(!source_file.parse_tree.has_value(), "Parse tree should be initially empty");

		// Get PST - this should trigger parsing
		source_file.getPST();

		// After parsing, parse tree should be cached
		assertTrue(source_file.parse_tree.has_value(), "Parse tree should be cached after getPST()");

		// Cleanup
		fs::FileManager::deleteFile(temp_file);
	}

	void testContentCaching() {
		// Create test files
		auto content1 = "fn function1() {}";
		auto content2 = "fn function2() {}";

		auto temp_file1 = fs::FileManager::createRandomTempFile(content1);
		auto temp_file2 = fs::FileManager::createRandomTempFile(content2);

		ModuleID module_id = ModuleID::next();

		try {
			// Test basic file content first
			auto basic_content1 = temp_file1.getContent();

			// Create SourceFile objects - this will cache the content
			SourceFile source_file1(temp_file1, module_id);
			SourceFile source_file2(temp_file2, module_id);

			// Test getCachedContent on SourceFile objects
			auto cached_content1 = source_file1.getCachedContent();
			auto cached_content2 = source_file2.getCachedContent();

			ASSERT_EQUAL(content1, cached_content1.view().stringView());
			ASSERT_EQUAL(content2, cached_content2.view().stringView());

			// Test that content is actually cached (call again)
			auto cached_content1_again = source_file1.getCachedContent();
			ASSERT_EQUAL(content1, cached_content1_again.view().stringView());

			// Test with same file path - create new SourceFile with same path
			SourceFile same_file_source(temp_file1, module_id);
			auto       cached_content_same = same_file_source.getCachedContent();
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
		auto     temp_file1 = fs::FileManager::createRandomTempFile("content1");
		auto     temp_file2 = fs::FileManager::createRandomTempFile("content2");
		ModuleID module_id  = ModuleID::next();

		SourceFile source_file1(temp_file1, module_id);
		SourceFile source_file2(temp_file2, module_id);

		// Test hash generation
		u64 hash1 = source_file1.queryUnstablePerfectHash();
		u64 hash2 = source_file2.queryUnstablePerfectHash();

		// Hashes should be different for different files
		assertTrue(hash1 != hash2, "Hashes should be different for different files");

		// Same file should return same hash
		u64 hash1_again = source_file1.queryUnstablePerfectHash();
		ASSERT_EQUAL(hash1, hash1_again);

		// Create another SourceFile with same path - should get same hash
		SourceFile source_file1_copy(temp_file1, module_id);
		u64        hash1_copy = source_file1_copy.queryUnstablePerfectHash();
		ASSERT_EQUAL(hash1, hash1_copy);

		// Cleanup
		fs::FileManager::deleteFile(temp_file1);
		fs::FileManager::deleteFile(temp_file2);
	}

	void testMultipleSourceFiles() {
		// Test working with multiple source files
		std::vector<fs::File>                    temp_files;
		std::vector<std::unique_ptr<SourceFile>> source_files;
		ModuleID                                 module_id = ModuleID::next();

		// Create multiple source files
		for (int i = 0; i < 5; ++i) {
			auto content
				= "fn function" + std::to_string(i) + "() { return " + std::to_string(i) + "; }";
			auto temp_file = fs::FileManager::createRandomTempFile(content);
			temp_files.push_back(temp_file);
			source_files.push_back(std::make_unique<SourceFile>(temp_file, module_id));
		}

		// Test that all files have unique IDs
		std::set<FileID> unique_ids;
		for (const auto& source_file: source_files) {
			assertTrue(
				unique_ids.find(source_file->id) == unique_ids.end(), "FileIDs should be unique"
			);
			unique_ids.insert(source_file->id);
		}

		// Test that all files belong to the same module
		for (const auto& source_file: source_files)
			ASSERT_EQUAL(module_id, source_file->linked_module);

		// Test PST generation for all files
		for (auto& source_file: source_files) {
			source_file->getPST();
			assertTrue(source_file->parse_tree.has_value(), "Parse tree should be cached");
		}

		// Test unique hashes
		std::set<u64> unique_hashes;
		for (auto& source_file: source_files) {
			u64 hash = source_file->queryUnstablePerfectHash();
			assertTrue(unique_hashes.find(hash) == unique_hashes.end(), "Hashes should be unique");
			unique_hashes.insert(hash);
		}

		// Cleanup
		for (const auto& temp_file: temp_files) fs::FileManager::deleteFile(temp_file);
	}

	void testSourceFileWithDifferentModules() {
		// Test source files belonging to different modules
		auto temp_file = fs::FileManager::createRandomTempFile("fn shared_function() {}");

		ModuleID module1 = ModuleID::next();
		ModuleID module2 = ModuleID::next();

		SourceFile source_file1(temp_file, module1);
		SourceFile source_file2(temp_file, module2);

		// Should have different IDs even with same file path
		assertTrue(source_file1.id != source_file2.id, "Should have different FileIDs");

		// Should belong to different modules
		ASSERT_EQUAL(module1, source_file1.linked_module);
		ASSERT_EQUAL(module2, source_file2.linked_module);
		assertTrue(
			source_file1.linked_module != source_file2.linked_module,
			"Should belong to different modules"
		);

		// Should have different hashes
		u64 hash1 = source_file1.queryUnstablePerfectHash();
		u64 hash2 = source_file2.queryUnstablePerfectHash();
		assertTrue(hash1 != hash2, "Should have different hashes");

		// Cleanup
		fs::FileManager::deleteFile(temp_file);
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/frontend/module_tree/tests/");
