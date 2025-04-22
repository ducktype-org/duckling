#include <filesystem/file.hpp>
#include <tester/tester.hpp>
#include <filesystem/vfs.hpp>

class SimpleFileSystemTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SimpleFileSystemTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(getSimpleContentTest);
		TESTER_ADD_TEST(filePathTest);
		TESTER_ADD_TEST(vfsTest);
	}

private:
	void getSimpleContentTest() {
		auto a_content = fs::getSimpleFileContent(path("a_file.txt"));

		assertTrue(a_content.view().size() == 12, "Wrong a_content file size");
		assertTrue(
			a_content.view().stringView() == "abrakadabra\n", "Wrong a_content file content"
		);
	}

	void filePathTest() {
		auto a_good_content = fs::getSimpleFileContent(path("a_file.txt"));
		auto b_good_content = fs::getSimpleFileContent(path("b_file.txt"));

		// loops twice to see if behavior is ok after all previous fileContents where destroyed
		for (i32 i = 0; i < 2; i++) {
			fs::FilePath a1(path("a_file.txt"));
			fs::FilePath a2(path("a_file.txt"));
			fs::FilePath b1(path("b_file.txt"));

			auto a1_content = a1.getContent();
			auto a2_content = a1.getContent();
			auto b1_content = b1.getContent();

			assertTrue(
				a1_content.view().getBegin() == a1.getContent().view().getBegin(),
				"a_file was read multiple times when it shouldn't"
			);
			assertTrue(
				a2_content.view().getBegin() == a1.getContent().view().getBegin(),
				"a_file was read multiple times when it shouldn't"
			);
			assertTrue(
				a1_content.view().getBegin() == a2_content.view().getBegin(),
				"a_file was read multiple times when it shouldn't"
			);

			assertTrue(
				a1_content.view().stringView() == a_good_content.view().stringView(),
				"Wrong a_file content"
			);
			assertTrue(
				b1_content.view().stringView() == b_good_content.view().stringView(),
				"Wrong b_file content"
			);
		}
	}

	void vfsTest() {
        VFS vfs;

        // Test createDirectory and listDirectory
        assertTrue(vfs.createDirectory("/testDir"), "Failed to create directory");
        assertTrue(vfs.createDirectory("/testDir/subDir"), "Failed to create subdirectory");
        auto dirContents = vfs.listDirectory("/testDir");
        assertTrue(dirContents.size() == 1 && dirContents[0] == "subDir", "Directory contents incorrect");

        // Test createFile and exists
        assertTrue(vfs.createFile("/testDir/file.txt"), "Failed to create file");
        assertTrue(vfs.exists("/testDir/file.txt"), "File does not exist");

        // Test writeFile and readFile
        assertTrue(vfs.writeFile("/testDir/file.txt", "Hello, VFS!"), "Failed to write to file");
        auto fileContent = vfs.readFile("/testDir/file.txt");
        assertTrue(fileContent == "Hello, VFS!", "File content incorrect");

        // Test isFile and isDirectory
        assertTrue(vfs.isFile("/testDir/file.txt"), "Path is not recognized as a file");
        assertTrue(vfs.isDirectory("/testDir"), "Path is not recognized as a directory");

        // Test getRootPath
        assertTrue(vfs.getRootPath() == "vroot", "Root path incorrect");

        // Test edge cases
        assertTrue(!vfs.createFile("/"), "Should not allow file creation at root");
        assertTrue(!vfs.createDirectory("/testDir/file.txt"), "Should not allow directory creation at file path");
    }
};

TESTER_COMMON_MAIN("/common/filesystem/tests/");
