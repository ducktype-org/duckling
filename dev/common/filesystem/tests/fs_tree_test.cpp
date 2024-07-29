#include <tester/tester.hpp>
#include "filesystem/fs_tree.hpp"

class FileSystemFsTreeTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS FileSystemFsTreeTest

	// This regex catches anything, that starts with '.' or '$'.
	const std::regex test_regex = std::regex(R"(\..*|\$.*)");

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("FileSystem FsTree test") {
		TESTER_ADD_TEST(parseDirectory);
		TESTER_ADD_TEST(testOtherFeatures);
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
		const auto root = fs::FilePath(path("test_directory_tree"));
		const auto fst  = fs::FsTree::create(root, test_regex, test_regex);

		ASSERT_EQUAL(false, fst->isEmpty());
		ASSERT_EQUAL(true, fst->getRoot() == root);
		ASSERT_EQUAL(true, fst->getParentTree().empty());
		ASSERT_EQUAL(
			true,
			fst->getDirs()["another_directory"]->getParentTree().value().getRoot() == fst->getRoot()
		);
	}
};

TESTER_COMMON_MAIN("common/filesystem/tests/");
