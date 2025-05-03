#include "../src_private/filesystem_private/vfs.hpp"

#include <filesystem/file.hpp>
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
		assertTrue(a_content.view().stringView() == "abrakadabra\n", "Wrong a_content file content");
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

		// Test createDirectory and listDirectory with real paths
		assertTrue(!vfs.createDirectory("/testDir"), "Not a virtual path");
		// Test getRootPath
		assertTrue(vfs.getRootPath() == "vfs:", "Root path incorrect");

		// Test edge cases with real paths
		assertTrue(!vfs.createFile("vfs:"), "Should not allow file creation at root");

		// Test virtual paths
		assertTrue(vfs.createDirectory("vfs:/virtualDir"), "Failed to create virtual directory");
		assertTrue(
			vfs.createDirectory("vfs:/virtualDir/virtualSubDir"),
			"Failed to create virtual subdirectory"
		);
		assertTrue(
			vfs.createDirectory("vfs:/test/random/long/path"),
			"Failed to create virtual subdirectory"
		);
		auto virtual_dir_contents = vfs.listDirectory("vfs:/virtualDir");
		assertTrue(
			virtual_dir_contents.size() == 1 && virtual_dir_contents[0] == "virtualSubDir",
			"Virtual directory contents incorrect"
		);

		assertTrue(
			vfs.createFile("vfs:/virtualDir/virtualFile.txt"), "Failed to create virtual file"
		);
		assertTrue(vfs.exists("vfs:/virtualDir/virtualFile.txt"), "Virtual file does not exist");

		assertTrue(
			vfs.writeFile("vfs:/virtualDir/virtualFile.txt", "Hello, Virtual VFS!"),
			"Failed to write to virtual file"
		);
		auto virtual_file_content = vfs.readFile("vfs:/virtualDir/virtualFile.txt");
		assertTrue(virtual_file_content == "Hello, Virtual VFS!", "Virtual file content incorrect");

		assertTrue(
			vfs.isFile("vfs:/virtualDir/virtualFile.txt"), "Virtual path is not recognized as a file"
		);
		assertTrue(
			vfs.isDirectory("vfs:/virtualDir"), "Virtual path is not recognized as a directory"
		);

		// Test edge cases with virtual paths
		assertTrue(!vfs.createFile("vfs:/"), "Should not allow file creation at virtual root");
		assertTrue(
			!vfs.createDirectory("vfs:/virtualDir/virtualFile.txt"),
			"Should not allow directory creation at virtual file path"
		);
	}

	void virtualFileTest() {
		// Create a virtual directory
		auto virtual_dir = fs::FilePath::createVirtualDirectory();
		assertTrue(virtual_dir.isDirectory(), "Virtual directory was not created correctly");
		assertTrue(
			virtual_dir.strView().find("vfs:") != std::string::npos,
			"Virtual directory path is incorrect"
		);

		// Create a virtual file inside the directory
		auto virtual_file = virtual_dir.createFileIn("Hello, Virtual File!", "testFile.txt");
		assertTrue(virtual_file.isFile(), "Virtual file was not created correctly");
		assertTrue(virtual_file.name() == "testFile.txt", "Virtual file name is incorrect");

		// List directory contents
		auto dir_contents = virtual_dir.listFilePaths();
		assertTrue(dir_contents.size() == 1, "Virtual directory should contain one file");
		assertTrue(dir_contents[0].name() == "testFile.txt", "Directory listing is incorrect");

		// Create a subdirectory
		auto sub_dir = virtual_dir.createDirectoryIn("subDir");
		assertTrue(sub_dir.isDirectory(), "Subdirectory was not created correctly");
		assertTrue(sub_dir.name() == "subDir", "Subdirectory name is incorrect");

		// List directory contents again
		dir_contents = virtual_dir.listFilePaths();
		assertTrue(dir_contents.size() == 2, "Virtual directory should contain two entries");
		assertTrue(
			std::ranges::find_if(
				dir_contents, [](const auto& entry) { return entry.name() == "subDir"; }
			) != dir_contents.end(),
			"Subdirectory is missing in directory listing"
		);

		// Test parent path
		assertTrue(
			sub_dir.parentPath().absolutePath() == virtual_dir.absolutePath(),
			"Parent path is incorrect"
		);

		// Test file modification time (should throw for virtual files)
		try {
			(void) virtual_file.getModifyTime();
			assertTrue(false, "getModifyTime should throw for virtual files");
		} catch (const base::LogicError&) {
			// Expected behavior
		}

		// Test creating a file with duplicate name (should throw)
		try {
			(void) virtual_dir.createFileIn("Duplicate content", "testFile.txt");
			assertTrue(false, "Creating a file with duplicate name should throw");
		} catch (const base::LogicError&) {
			// Expected behavior
		}
	}
};

TESTER_COMMON_MAIN("/common/filesystem/tests/");
