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
		TESTER_ADD_TEST(fileOperationsTest);
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
			fs::File a1(path("a_file.txt"));
			fs::File a2(path("a_file.txt"));
			fs::File b1(path("b_file.txt"));

			auto a1_content = a1.getContent();
			auto a2_content = a1.getContent();
			auto b1_content = b1.getContent();

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
		Ref<fs::VFS> vfs = fs::VFS::getInstance();

		// Test createDirectory and listDirectory with real paths
		try {
			vfs->createDirectory("/testDir");
			assertTrue(false, "Expected LogicError for non-virtual path");
		} catch (const base::LogicError& e) {
			// Expected behavior
			assertTrue(
				std::string(e.what()) == "Path is not a vfs path!", "Unexpected error message"
			);
		}

		// Test getRootPath
		assertTrue(vfs->getRootPath() == "vfs:", "Root path incorrect");

		// Test edge cases with real paths
		assertTrue(!vfs->createFile("vfs:"), "Should not allow file creation at root");

		// Test virtual paths
		assertTrue(vfs->createDirectory("vfs:/virtualDir"), "Failed to create virtual directory");
		assertTrue(
			vfs->createDirectory("vfs:/virtualDir/virtualSubDir"),
			"Failed to create virtual subdirectory"
		);
		assertTrue(
			vfs->createDirectory("vfs:/test/random/long/path"),
			"Failed to create virtual subdirectory"
		);
		auto virtual_dir_contents = vfs->listDirectory("vfs:/virtualDir");
		assertTrue(
			virtual_dir_contents.size() == 1 && virtual_dir_contents[0] == "virtualSubDir",
			"Virtual directory contents incorrect"
		);

		assertTrue(
			vfs->createFile("vfs:/virtualDir/virtualFile.txt"), "Failed to create virtual file"
		);
		assertTrue(vfs->exists("vfs:/virtualDir/virtualFile.txt"), "Virtual file does not exist");

		assertTrue(
			vfs->writeFile("vfs:/virtualDir/virtualFile.txt", "Hello, Virtual VFS!"),
			"Failed to write to virtual file"
		);
		auto virtual_file_content = vfs->readFile("vfs:/virtualDir/virtualFile.txt");
		assertTrue(virtual_file_content == "Hello, Virtual VFS!", "Virtual file content incorrect");

		assertTrue(
			vfs->isFile("vfs:/virtualDir/virtualFile.txt"),
			"Virtual path is not recognized as a file"
		);
		assertTrue(
			vfs->isDirectory("vfs:/virtualDir"), "Virtual path is not recognized as a directory"
		);

		// Test edge cases with virtual paths
		assertTrue(!vfs->createFile("vfs:/"), "Should not allow file creation at virtual root");
		assertTrue(
			!vfs->createDirectory("vfs:/virtualDir/virtualFile.txt"),
			"Should not allow directory creation at virtual file path"
		);

		// Test deleteFile
		assertTrue(
			vfs->deleteFile("vfs:/virtualDir/virtualFile.txt"), "Failed to delete virtual file"
		);

		assertTrue(
			!vfs->exists("vfs:/virtualDir/virtualFile.txt"),
			"Virtual file still exists after deletion"
		);

		// Test deleteDirectory without force
		assertTrue(
			!vfs->deleteDirectory("vfs:/virtualDir", false),
			"Should not delete non-empty directory without force"
		);

		// Test deleteDirectory with force
		assertTrue(
			vfs->deleteDirectory("vfs:/virtualDir", true), "Failed to delete directory with force"
		);

		assertTrue(
			!vfs->exists("vfs:/virtualDir"), "Directory still exists after deletion with force"
		);
	}

	void virtualFileTest() {
		// Create a virtual directory
		auto virtual_dir = fs::FileManager::createVirtualDirectory();
		assertTrue(virtual_dir.isDirectory(), "Virtual directory was not created correctly");
		assertTrue(
			virtual_dir.strView().find("vfs:") != std::string::npos,
			"Virtual directory path is incorrect"
		);

		// Create a virtual file inside the directory
		auto virtual_file
			= fs::FileManager::createFileIn(virtual_dir, "Hello, Virtual File!", "testFile.txt");
		assertTrue(virtual_file.isFile(), "Virtual file was not created correctly");
		assertTrue(virtual_file.name() == "testFile.txt", "Virtual file name is incorrect");

		// List directory contents
		auto dir_contents = virtual_dir.listFilePaths();
		assertTrue(dir_contents.size() == 1, "Virtual directory should contain one file");
		assertTrue(dir_contents[0].name() == "testFile.txt", "Directory listing is incorrect");

		// Create a subdirectory
		auto sub_dir = fs::FileManager::createDirectoryIn(virtual_dir, "subDir");
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
		} catch (const base::Panic&) {
			// Expected behavior
		}

		// Test creating a file with duplicate name (should throw)
		try {
			(void) fs::FileManager::createFileIn(virtual_dir, "Duplicate content", "testFile.txt");
			assertTrue(false, "Creating a file with duplicate name should throw");
		} catch (const base::Panic&) {
			// Expected behavior
		}
	}

	void fileOperationsTest() {
		// Test writeToFile functionality
		auto temp_file = fs::FileManager::createRandomTempFile("Initial content");
		assertTrue(temp_file.isFile(), "Temp file was not created correctly");
		assertTrue(temp_file.getType() == fs::FileType::Temporary, "File should be temporary");

		// Test reading content
		auto content = temp_file.getContent();
		assertTrue(content.view().stringView() == "Initial content", "Initial content incorrect");

		// Test overwriting file content
		temp_file.writeToFile("New content", false);
		auto new_content = temp_file.getContent();
		assertTrue(new_content.view().stringView() == "New content", "Content was not overwritten");

		// Test appending to file content
		temp_file.writeToFile(" appended", true);
		auto appended_content = temp_file.getContent();
		assertTrue(
			appended_content.view().stringView() == "New content appended",
			"Content was not appended"
		);

		// Test virtual file writing
		auto virtual_file = fs::FileManager::createRandomVirtualFile("Virtual initial");
		assertTrue(virtual_file.isFile(), "Virtual file was not created correctly");
		assertTrue(virtual_file.getType() == fs::FileType::Virtual, "File should be virtual");

		auto virtual_content = virtual_file.getContent();
		assertTrue(
			virtual_content.view().stringView() == "Virtual initial",
			"Virtual initial content incorrect"
		);

		// Test overwriting virtual file (append should fail)
		virtual_file.writeToFile("Virtual new content");
		auto virtual_new_content = virtual_file.getContent();
		assertTrue(
			virtual_new_content.view().stringView() == "Virtual new content",
			"Virtual content was not overwritten"
		);

		// Test that append fails for virtual files
		try {
			virtual_file.writeToFile(" should fail", true);
			assertTrue(false, "Append should fail for virtual files");
		} catch (const base::Panic&) {
			// Expected behavior
		}

		// Test file deletion
		assertTrue(fs::FileManager::fileExists(temp_file), "Temp file should exist before deletion");
		assertTrue(fs::FileManager::deleteFile(temp_file), "Failed to delete temp file");
		assertTrue(
			!fs::FileManager::fileExists(temp_file), "Temp file should not exist after deletion"
		);

		assertTrue(
			fs::FileManager::fileExists(virtual_file), "Virtual file should exist before deletion"
		);
		assertTrue(fs::FileManager::deleteFile(virtual_file), "Failed to delete virtual file");
		assertTrue(
			!fs::FileManager::fileExists(virtual_file),
			"Virtual file should not exist after deletion"
		);

		// Test directory deletion
		auto temp_dir    = fs::FileManager::createTempDirectory();
		auto file_in_dir = fs::FileManager::createFileIn(temp_dir, "content", "test.txt");
		auto sub_dir     = fs::FileManager::createDirectoryIn(temp_dir, "subdir");

		assertTrue(fs::FileManager::folderExists(temp_dir), "Temp directory should exist");

		// Test deletion without force (should throw exception for non-empty directory)
		try {
			fs::FileManager::deleteFolder(temp_dir, false);
			assertTrue(
				false, "Should throw exception when deleting non-empty directory without force"
			);
		} catch (const std::filesystem::filesystem_error&) {
			// Expected behavior - directory should still exist
			assertTrue(
				fs::FileManager::folderExists(temp_dir),
				"Directory should still exist after failed deletion"
			);
		}

		// Test deletion with force
		assertTrue(
			fs::FileManager::deleteFolder(temp_dir, true), "Failed to delete directory with force"
		);
		assertTrue(
			!fs::FileManager::folderExists(temp_dir),
			"Directory should not exist after deletion with force"
		);

		// Test file utilities
		auto test_file = fs::FileManager::createRandomTempFile();
		assertTrue(test_file.stem() != "", "File stem should not be empty");
		assertTrue(test_file.extension() == "", "File should have no extension");
		assertTrue(test_file.uri().starts_with("file://"), "URI should start with file://");
		assertTrue(test_file.absolutePath() != "", "Absolute path should not be empty");

		// Test getContentSafe
		auto safe_content = test_file.getContentSafe();
		assertTrue(safe_content.has_value(), "getContentSafe should succeed for existing file");

		// Delete file and test getContentSafe again
		fs::FileManager::deleteFile(test_file);
		auto safe_content_after_delete = test_file.getContentSafe();
		assertTrue(
			!safe_content_after_delete.has_value(),
			"getContentSafe should fail for non-existing file"
		);

		// Test path conversions
		auto virtual_dir = fs::FileManager::createVirtualDirectory();
		assertTrue(
			fs::VFS::isVirtualPath(virtual_dir.absolutePath()), "Should recognize virtual path"
		);

		// Test physical file creation with override
		auto current_dir   = std::filesystem::current_path();
		auto physical_path = current_dir / "test_physical.txt";

		auto physical_file1 = fs::FileManager::createPhysicalFile(physical_path, "content1", false);
		assertTrue(physical_file1.getType() == fs::FileType::Physical, "File should be physical");

		// Test override functionality
		auto physical_file2 = fs::FileManager::createPhysicalFile(physical_path, "content2", true);
		auto overridden_content = physical_file2.getContent();
		assertTrue(
			overridden_content.view().stringView() == "content2", "Content should be overridden"
		);

		// Test trying to create without override (should fail)
		try {
			fs::FileManager::createPhysicalFile(physical_path, "content3", false);
			assertTrue(false, "Should fail to create existing file without override");
		} catch (const base::Panic&) {
			// Expected behavior
		}

		// Test symlink detection (should be false for all our test files)
		assertTrue(!test_file.isSymlink(), "Test files should not be symlinks");
		assertTrue(
			!fs::FileManager::isSymlink(physical_path), "Physical file should not be symlink"
		);

		// Cleanup physical file
		std::filesystem::remove(physical_path);

		// Cleanup
		fs::FileManager::deleteFolder(virtual_dir, true);
	}
};

TESTER_COMMON_MAIN("/src/common/filesystem/tests/");
