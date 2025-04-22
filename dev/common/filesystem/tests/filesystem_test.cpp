#include <filesystem/file.hpp>
#include <filesystem/vfs.hpp>
#include <tester/tester.hpp>

class SimpleFileSystemTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SimpleFileSystemTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(getSimpleContentTest);
		TESTER_ADD_TEST(filePathTest);
		TESTER_ADD_TEST(vfsTest);
		TESTER_ADD_TEST(virtualFileTest);
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
		assertTrue(
			dirContents.size() == 1 && dirContents[0] == "subDir", "Directory contents incorrect"
		);

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
		assertTrue(
			!vfs.createDirectory("/testDir/file.txt"),
			"Should not allow directory creation at file path"
		);
	}

	void virtualFileTest() {
		// Create a virtual directory
		auto virtualDir = fs::FilePath::createVirtualDirectory();
		assertTrue(virtualDir.isDirectory(), "Virtual directory was not created correctly");
		assertTrue(
			virtualDir.strView().find("vroot") != std::string::npos,
			"Virtual directory path is incorrect"
		);

		// Create a virtual file inside the directory
		auto virtualFile = virtualDir.createFileIn("Hello, Virtual File!", "testFile.txt");
		assertTrue(virtualFile.isFile(), "Virtual file was not created correctly");
		assertTrue(virtualFile.name() == "testFile.txt", "Virtual file name is incorrect");

		// Check file content
		auto content = virtualFile.getContent();
		assertTrue(
			content.view().stringView() == "Hello, Virtual File!",
			"Virtual file content is incorrect"
		);

		// List directory contents
		auto dirContents = virtualDir.listDirectory();
		assertTrue(dirContents.size() == 1, "Virtual directory should contain one file");
		assertTrue(dirContents[0] == "testFile.txt", "Directory listing is incorrect");

		// Create a subdirectory
		auto subDir = virtualDir.createDirectoryIn("subDir");
		assertTrue(subDir.isDirectory(), "Subdirectory was not created correctly");
		assertTrue(subDir.name() == "subDir", "Subdirectory name is incorrect");

		// List directory contents again
		dirContents = virtualDir.listDirectory();
		assertTrue(dirContents.size() == 2, "Virtual directory should contain two entries");
		assertTrue(
			std::ranges::find(dirContents, "subDir") != dirContents.end(),
			"Subdirectory is missing in directory listing"
		);

		// Test parent path
		assertTrue(
			subDir.parentPath().absolutePath() == virtualDir.absolutePath(),
			"Parent path is incorrect"
		);

		// Test file modification time (should throw for virtual files)
		try {
			(void) virtualFile.getModifyTime();
			assertTrue(false, "getModifyTime should throw for virtual files");
		} catch (const base::LogicError&) {
			// Expected behavior
		}

		// Test creating a file with duplicate name (should throw)
		try {
			(void) virtualDir.createFileIn("Duplicate content", "testFile.txt");
			assertTrue(false, "Creating a file with duplicate name should throw");
		} catch (const base::LogicError&) {
			// Expected behavior
		}
	}
};

TESTER_COMMON_MAIN("/common/filesystem/tests/");
