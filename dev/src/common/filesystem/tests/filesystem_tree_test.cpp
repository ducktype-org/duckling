#include <filesystem/fs_tree.hpp>
#include <tester/tester.hpp>

class FileSystemFsTreeTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS FileSystemFsTreeTest

	// This regex catches anything, that starts with '.' or '$'.
	const std::regex test_regex = std::regex(R"(\..*|\$.*)");

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(parseDirectory);
		TESTER_ADD_TEST(testOtherFeatures);
		TESTER_ADD_TEST(testVirtualFiles);
	}

private:
	void parseDirectory() {
		const auto fst = fs::FsTree::create(path("test_directory_tree"), test_regex, test_regex);

		ASSERT_EQUAL(2, fst->getFiles().size());
		ASSERT_EQUAL(1, fst->getDirs().size());

		ASSERT_EQUAL(false, fst->getDirs().contains(".skipped_directory"));
		ASSERT_EQUAL(true, fst->getDirs().contains("another_directory"));

		ASSERT_EQUAL(false, fst->getFiles().contains(".skipped_file"));
		ASSERT_EQUAL(true, fst->getFiles().contains("file"));
		ASSERT_EQUAL(true, fst->getFiles().contains("file.txt"));
		ASSERT_EQUAL("content\n", fst->getFiles()["file.txt"].getContent().view());

		auto another_directory = fst->getDirs()["another_directory"];
		ASSERT_EQUAL(0, another_directory->getFiles().size());
	}

	void testOtherFeatures() {
		const auto root = fs::File(path("test_directory_tree"));
		const auto fst  = fs::FsTree::create(root, test_regex, test_regex);

		ASSERT_EQUAL(false, fst->isEmpty());
		ASSERT_EQUAL(true, fst->getRoot() == root);
		ASSERT_EQUAL(true, fst->getParentTree().empty());
		ASSERT_EQUAL(
			true,
			fst->getDirs()["another_directory"]->getParentTree().value()->getRoot() == fst->getRoot()
		);
	}

	void testVirtualFiles() {
		// Create a virtual root directory
		auto root = fs::FileManager::createVirtualDirectory();

		// Create subdirectories and files
		auto sub_dir1 = fs::FileManager::createDirectoryIn(root, "subDir1");
		auto sub_dir2 = fs::FileManager::createDirectoryIn(root, "subDir2");
		auto file1    = fs::FileManager::createFileIn(root, "File1 content", "file1.txt");
		auto file2    = fs::FileManager::createFileIn(sub_dir1, "File2 content", "file2.txt");

		// Create FsTree from the virtual root directory
		auto fst = fs::FsTree::create(root);

		// Test getRoot()
		ASSERT_EQUAL(true, fst->getRoot() == root);

		// Test isEmpty()
		ASSERT_EQUAL(false, fst->isEmpty());

		// Test getDirs()
		const auto& dirs = fst->getDirs();
		ASSERT_EQUAL(2, dirs.size());
		ASSERT_EQUAL(true, dirs.contains("subDir1"));
		ASSERT_EQUAL(true, dirs.contains("subDir2"));

		// Test getFiles()
		const auto& files = fst->getFiles();
		ASSERT_EQUAL(1, files.size());
		ASSERT_EQUAL(true, files.contains("file1.txt"));
		ASSERT_EQUAL("File1 content", files.at("file1.txt").getContent().view());

		// Test subdirectory tree
		auto        sub_tree  = dirs.at("subDir1");
		const auto& sub_files = sub_tree->getFiles();
		ASSERT_EQUAL(1, sub_files.size());
		ASSERT_EQUAL(true, sub_files.contains("file2.txt"));
		ASSERT_EQUAL("File2 content", sub_files.at("file2.txt").getContent().view());

		// Test prettyPrint()
		auto tree_representation = fst->prettyPrint();
		ASSERT_EQUAL(true, tree_representation.find("subDir1") != std::string::npos);
		ASSERT_EQUAL(true, tree_representation.find("file1.txt") != std::string::npos);
	}
};

TESTER_COMMON_MAIN("/src/common/filesystem/tests/");
