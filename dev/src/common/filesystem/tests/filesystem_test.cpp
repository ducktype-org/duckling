#include <filesystem_private/vfs.hpp>

#include <filesystem/file.hpp>
#include <tester/tester.hpp>

#include <algorithm>
#include <any>
#include <chrono>
#include <fstream>

class SimpleFileSystemTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SimpleFileSystemTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(getSimpleContentTest);
		TESTER_ADD_TEST(filePathTest);
		TESTER_ADD_TEST(vfsTest);
		TESTER_ADD_TEST(vfsMetadataTest);
		TESTER_ADD_TEST(virtualFileTest);
		TESTER_ADD_TEST(fileOperationsTest);
		TESTER_ADD_TEST(fileManagerTest);
		TESTER_ADD_TEST(fileTest);
	}

private:
	void getSimpleContentTest() {
		auto a_content = fs::File(path("a_file.txt")).getContent();

		assertTrue(a_content.view().size() == 12, "Wrong a_content file size");
		assertTrue(a_content.view().stringView() == "abrakadabra\n", "Wrong a_content file content");
	}

	void filePathTest() {
		auto a_good_content = fs::File(path("a_file.txt")).getContent();
		auto b_good_content = fs::File(path("b_file.txt")).getContent();

		// loops twice to see if behavior is ok after all previous base::SharedView where destroyed
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

		// Test FilePath::canonical() on virtual path (should throw)
		try {
			fs::FilePath vpath("vfs:/some_virtual_path");
			std::ignore = vpath.canonical();
			assertTrue(false, "canonical() on virtual path should throw");
		} catch (const base::Panic&) {
			// Expected
		}

		// Test FilePath::join(const FilePath&)
		fs::FilePath p1("abc");
		fs::FilePath p2("def");
		auto         joined = p1.join(p2);
		assertTrue(joined.string().find("def") != std::string::npos, "join(FilePath) failed");

		// Test FilePath::operator/(const FilePath&)
		auto joined2 = p1 / p2;
		assertTrue(joined2.string().find("def") != std::string::npos, "operator/ failed");

		// Test FilePath::absolute() on virtual path (should return itself)
		fs::FilePath vpath2("vfs:/abs_test");
		auto         abs_vpath = vpath2.absolute();
		assertTrue(abs_vpath == vpath2, "absolute() on virtual path should return itself");
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

		// Test append functionality
		assertTrue(
			vfs->writeFile("vfs:/virtualDir/virtualFile.txt", " Appended!", true),
			"Failed to append to virtual file"
		);
		auto appended_content = vfs->readFile("vfs:/virtualDir/virtualFile.txt");
		assertTrue(
			appended_content == "Hello, Virtual VFS! Appended!",
			"Virtual file append content incorrect"
		);

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

	/// A stand-in for what a user of the filesystem (the language server, say) would attach.
	struct FileInfo {
		std::string language_id;
		i32         version;
	};

	void vfsMetadataTest() {
		Ref<fs::VFS> vfs = fs::VFS::getInstance();

		// A directory of our own, so the shared VFS singleton cannot leak state between tests.
		auto dir       = fs::FileManager::createRandomVirtualDirectory();
		auto dir_path  = dir.getFilePath().getPath();
		auto file_path = dir_path / "metadata_file.dk";
		assertTrue(vfs->createFile(file_path), "Failed to create virtual file");

		// A fresh file has no metadata.
		assertTrue(!vfs->hasFileMetadata(file_path), "A fresh virtual file has no metadata");
		assertTrue(!vfs->readFileMetadata(file_path).has_value(), "Metadata starts out empty");

		// Write, then read it back.
		assertTrue(vfs->writeFileMetadata(file_path, i32(7)), "Failed to write metadata");
		assertTrue(vfs->hasFileMetadata(file_path), "Metadata should be present after writing");
		assertEqual(i32(7), base::anyCast<i32>(vfs->readFileMetadata(file_path)), "Wrong metadata");

		// Writing again replaces the old value, type included.
		assertTrue(
			vfs->writeFileMetadata(file_path, std::string("duckling")), "Failed to rewrite metadata"
		);
		assertEqual(
			std::string("duckling"),
			base::anyCast<std::string>(vfs->readFileMetadata(file_path)),
			"Metadata was not replaced"
		);

		// Content and metadata are independent.
		assertTrue(vfs->writeFile(file_path, "let x = 1"), "Failed to write file content");
		assertEqual(
			std::string("duckling"),
			base::anyCast<std::string>(vfs->readFileMetadata(file_path)),
			"Writing content should not touch the metadata"
		);

		// Clearing leaves the file in place.
		assertTrue(vfs->clearFileMetadata(file_path), "Failed to clear metadata");
		assertTrue(!vfs->hasFileMetadata(file_path), "Metadata should be gone after clearing");
		assertTrue(vfs->isFile(file_path), "Clearing metadata should not delete the file");

		// Directories and missing files have no metadata to write.
		assertTrue(
			!vfs->writeFileMetadata(dir_path, i32(1)), "A directory should not accept metadata"
		);
		assertTrue(!vfs->hasFileMetadata(dir_path), "A directory should never report metadata");
		assertTrue(
			!vfs->writeFileMetadata(dir_path / "nope.dk", i32(1)),
			"A missing file should not accept metadata"
		);
		assertThrows<base::Panic>(
			[&] { std::ignore = vfs->readFileMetadata(dir_path / "nope.dk"); },
			"Reading metadata of a missing file should panic"
		);
		assertThrows<base::Panic>(
			[&] { std::ignore = vfs->readFileMetadata(dir_path); },
			"Reading metadata of a directory should panic"
		);
		assertThrows<base::LogicError>(
			[&] { std::ignore = vfs->readFileMetadata("/tmp/not_virtual.dk"); },
			"Reading metadata of a non-virtual path should throw"
		);

		// The same thing through the public `fs::File` API.
		auto file = dir.createSubFile("let y = 2", "through_file_api.dk");
		assertTrue(!file.hasMetadata(), "A fresh file should have no metadata");
		file.writeMetadata(FileInfo{ .language_id = "duckling", .version = 3 });
		assertTrue(file.hasMetadata(), "Metadata should be present after writing");

		auto info = file.getMetadataAs<FileInfo>();
		assertEqual(std::string("duckling"), info.language_id, "Wrong language id in metadata");
		assertEqual(i32(3), info.version, "Wrong version in metadata");

		// Asking for the wrong type reports it instead of silently succeeding.
		assertThrows<base::LogicError>(
			[&] { std::ignore = file.getMetadataAs<i32>(); },
			"Casting metadata to the wrong type should throw"
		);

		file.clearMetadata();
		assertTrue(!file.hasMetadata(), "Metadata should be gone after clearing");

		// Metadata is a VFS-only feature.
		auto temp_file = fs::FileManager::createRandomTempFile("content");
		assertThrows<base::Panic>(
			[&] { temp_file.writeMetadata(i32(1)); }, "A temporary file should not accept metadata"
		);
		assertThrows<base::Panic>(
			[&] { std::ignore = temp_file.getMetadata(); },
			"A temporary file should have no metadata to read"
		);

		// Deleting the file drops its metadata with it.
		file.writeMetadata(i32(42));
		assertTrue(fs::FileManager::deleteFile(file), "Failed to delete virtual file");
		assertTrue(vfs->createFile(file.getFilePath().getPath()), "Failed to recreate the file");
		assertTrue(
			!file.hasMetadata(), "A recreated file must not inherit the deleted file's metadata"
		);
	}

	void virtualFileTest() {
		// Create a virtual directory
		auto virtual_dir = fs::FileManager::createRandomVirtualDirectory();
		assertTrue(virtual_dir.isDirectory(), "Virtual directory was not created correctly");
		assertTrue(
			virtual_dir.getFilePath().strView().find("vfs:") != std::string::npos,
			"Virtual directory path is incorrect"
		);

		// Create a virtual file inside the directory
		auto virtual_file = virtual_dir.createSubFile("Hello, Virtual File!", "testFile.txt");
		assertTrue(virtual_file.isFile(), "Virtual file was not created correctly");
		assertTrue(virtual_file.name() == "testFile.txt", "Virtual file name is incorrect");

		// List directory contents
		auto dir_contents = virtual_dir.listFilePaths();
		assertTrue(dir_contents.size() == 1, "Virtual directory should contain one file");
		assertTrue(dir_contents[0].name() == "testFile.txt", "Directory listing is incorrect");

		// Create a subdirectory
		auto sub_dir = virtual_dir.createSubDirectory("subDir");
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
			sub_dir.getFilePath().parentPath().native() == virtual_dir.getFilePath().native(),
			"Parent path is incorrect"
		);

		// Test creating a file with duplicate name (should throw)
		try {
			std::ignore = virtual_dir.createSubFile("Duplicate content", "testFile.txt");
			assertTrue(false, "Creating a file with duplicate name should throw");
		} catch (const base::Panic&) {
			// Expected behavior
		}
	}

	void fileOperationsTest() {
		// Test directory deletion
		auto temp_dir    = fs::FileManager::createRandomTempDirectory();
		auto file_in_dir = temp_dir.createSubFile("content", "test.txt");
		auto sub_dir     = temp_dir.createSubFile("subdir");

		assertTrue(temp_dir.exists(), "Temp directory should exist");

		// Test deletion without force (should throw exception for non-empty directory)
		try {
			fs::FileManager::deleteFolder(temp_dir, false);
			assertTrue(
				false, "Should throw exception when deleting non-empty directory without force"
			);
		} catch (const std::filesystem::filesystem_error&) {
			// Expected behavior - directory should still exist
			assertTrue(temp_dir.exists(), "Directory should still exist after failed deletion");
		}

		// Test deletion with force
		assertTrue(
			fs::FileManager::deleteFolder(temp_dir, true), "Failed to delete directory with force"
		);
		assertTrue(!temp_dir.exists(), "Directory should not exist after deletion with force");

		// Test path conversions
		auto virtual_dir = fs::FileManager::createRandomVirtualDirectory();
		assertTrue(
			fs::VFS::isVirtualPath(virtual_dir.getFilePath().native()),
			"Should recognize virtual path"
		);

		// Cleanup
		fs::FileManager::deleteFolder(virtual_dir, true);
	}

	void fileManagerTest() {
		// FileManager specific tests from uncoveredCodeTest

		// Test default directory getters
		fs::File temp_default = fs::FilePath::getDefaultTempDirectoryPath();
		assertTrue(
			temp_default.getType() == fs::FileType::Temporary, "Default temp dir should be temporary"
		);

		fs::File virtual_default = fs::FilePath::getDefaultVirtualDirectoryPath();
		assertTrue(
			virtual_default.getType() == fs::FileType::Virtual,
			"Default virtual dir should be virtual"
		);

		// Test createFileIn with random name generation
		auto virtual_dir = fs::FileManager::createRandomVirtualDirectory();
		auto random_file = virtual_dir.createSubFile("random content");  // no custom_name
		assertTrue(random_file.isFile(), "Random file should be created");

		// Test physical folder creation with unique name
		auto current_dir = std::filesystem::current_path();
		auto unique_folder_name
			= "test_physical_folder_"
		    + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
		auto physical_folder_path = current_dir / unique_folder_name;

		// Ensure cleanup from any previous run
		std::filesystem::remove_all(physical_folder_path);

		auto physical_folder = fs::FileManager::createPhysicalFolder(physical_folder_path, false);
		assertTrue(physical_folder.isDirectory(), "Physical folder should be created");

		// Test physical folder with override
		auto physical_folder2 = fs::FileManager::createPhysicalFolder(physical_folder_path, true);
		assertTrue(
			physical_folder2.isDirectory(), "Physical folder should be recreated with override"
		);

		auto new_phisical_file = physical_folder2.createSubFile("content", "duplicate.txt");
		auto new_phisical_dir  = physical_folder2.createSubDirectory("some_dir");
		assertTrue(
			new_phisical_file.getType() == fs::FileType::Physical,
			"Physical file should be created in physical folder"
		);
		assertTrue(
			new_phisical_dir.getType() == fs::FileType::Physical,
			"Physical directory should be created in physical folder"
		);


		// Test virtual file creation with override
		auto virtual_file_path = virtual_dir.getFilePath().native() + "/allow_overwritetest.txt";
		fs::FileManager::createVirtualFile(virtual_file_path, "original");
		auto overridden_virtual
			= fs::FileManager::createVirtualFile(virtual_file_path, "overridden", true);
		assertTrue(
			overridden_virtual.getContent().view().stringView() == "overridden",
			"Virtual file should be overridden"
		);

		// Test virtual folder creation
		fs::FilePath virtual_folder_path
			= virtual_dir.getFilePath().genericString() + "/test_folder";
		auto virtual_folder = fs::FileManager::createVirtualFolder(virtual_folder_path);
		assertTrue(virtual_folder.isDirectory(), "Virtual folder should be created");

		// Test virtual folder with override
		auto virtual_folder2 = fs::FileManager::createVirtualFolder(virtual_folder_path, true);
		assertTrue(
			virtual_folder2.isDirectory(), "Virtual folder should be recreated with override"
		);

		// Test temp folder creation with unique name
		auto unique_temp_name
			= "test_temp_folder_"
		    + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
		fs::FilePath temp_folder_path = std::filesystem::temp_directory_path() / unique_temp_name;
		auto         temp_folder      = fs::FileManager::createTempFolder(temp_folder_path);
		assertTrue(temp_folder.isDirectory(), "Temp folder should be created");

		// Test temp folder with override
		auto temp_folder2 = fs::FileManager::createTempFolder(temp_folder_path, true);
		assertTrue(temp_folder2.isDirectory(), "Temp folder should be recreated with override");

		// Test folderExists with path
		assertTrue(virtual_folder_path.exists(), "Virtual folder should exist");
		assertTrue(temp_folder_path.exists(), "Temp folder should exist");

		// Test folderExists with virtual File
		assertTrue(virtual_folder.exists(), "Virtual folder File should exist");

		// Test path conversions
		fs::FilePath simple_physical_path = current_dir / "simple_test.txt";
		std::filesystem::remove(simple_physical_path);  // cleanup first
		std::ofstream simple_file(simple_physical_path.getPath());
		simple_file << "test";
		simple_file.close();

		fs::FilePath virtual_path = simple_physical_path.toVirtualPath();
		assertTrue(fs::VFS::isVirtualPath(virtual_path), "Should convert to virtual path");

		auto physical_path = virtual_path.toPhysicalPath();
		assertTrue(!fs::VFS::isVirtualPath(physical_path), "Should convert from virtual path");

		// Test error cases for path conversions
		try {
			std::ignore = fs::FilePath("vfs:/invalid").toVirtualPath();
			assertTrue(false, "Should fail to convert virtual path to virtual path");
		} catch (const base::Panic&) {
			// Expected behavior
		}

		try {
			std::ignore = temp_folder_path.toVirtualPath();
			assertTrue(false, "Should fail to convert temp path to virtual path");
		} catch (const base::Panic&) {
			// Expected behavior
		}

		try {
			std::ignore = simple_physical_path.toPhysicalPath();
			assertTrue(false, "Should fail to convert non-virtual path from virtual path");
		} catch (const base::Panic&) {
			// Expected behavior
		}

		// Test FileManager CORE_PANIC lines

		// Test createFileIn with duplicate name in virtual directory
		auto test_virtual_dir = fs::FileManager::createRandomVirtualDirectory();
		std::ignore           = test_virtual_dir.createSubFile("content", "duplicate.txt");
		try {
			std::ignore = test_virtual_dir.createSubFile("content", "duplicate.txt");
			assertTrue(false, "Should fail to create duplicate file in virtual directory");
		} catch (const base::Panic&) {
			// Expected: CORE_PANIC("File already exists in virtual directory: ...")
		}

		// Test createFileIn with duplicate name in physical directory
		auto test_temp_dir = fs::FileManager::createRandomTempDirectory();
		std::ignore        = test_temp_dir.createSubFile("content", "duplicate.txt");
		try {
			std::ignore = test_temp_dir.createSubFile("content", "duplicate.txt");
			assertTrue(false, "Should fail to create duplicate file in physical directory");
		} catch (const base::Panic&) {
			// Expected: CORE_PANIC("File already exists in directory: ...")
		}

		// Test createDirectoryIn with duplicate name in virtual directory
		std::ignore = test_virtual_dir.createSubDirectory("duplicate_dir");
		try {
			std::ignore = test_virtual_dir.createSubDirectory("duplicate_dir");
			assertTrue(false, "Should fail to create duplicate directory in virtual directory");
		} catch (const base::Panic&) {
			// Expected: CORE_PANIC("Directory already exists in virtual directory: ...")
		}

		// Test createDirectoryIn with duplicate name in physical directory
		std::ignore = test_temp_dir.createSubDirectory("duplicate_dir");
		try {
			std::ignore = test_temp_dir.createSubDirectory("duplicate_dir");
			assertTrue(false, "Should fail to create duplicate directory in physical directory");
		} catch (const base::Panic&) {
			// Expected: CORE_PANIC("Directory already exists: ...")
		}

		// FileManager tests from fileOperationsTest

		// Test file/folder existence checks
		auto temp_file = fs::FileManager::createRandomTempFile("Test content");
		assertTrue(temp_file.exists(), "File should exist");

		auto temp_dir = fs::FileManager::createRandomTempDirectory();
		assertTrue(temp_dir.exists(), "Folder should exist");

		// Test file deletion
		assertTrue(fs::FileManager::deleteFile(temp_file), "Should delete file");
		assertTrue(!temp_file.exists(), "File should not exist after deletion");

		// Test folder deletion
		assertTrue(fs::FileManager::deleteFolder(temp_dir, true), "Should delete folder");
		assertTrue(!temp_dir.exists(), "Folder should not exist after deletion");

		// Test physical file creation with override
		fs::FilePath physical_file_path = current_dir / "test_physical.txt";
		auto         physical_file1
			= fs::FileManager::createPhysicalFile(physical_file_path, "content1", false);
		assertTrue(physical_file1.getType() == fs::FileType::Physical, "File should be physical");

		// Test override functionality
		auto physical_file2
			= fs::FileManager::createPhysicalFile(physical_file_path, "content2", true);
		auto overridden_content = physical_file2.getContent();
		assertTrue(
			overridden_content.view().stringView() == "content2", "Content should be overridden"
		);

		// Test trying to create without override (should fail)
		try {
			fs::FileManager::createPhysicalFile(physical_file_path, "content3", false);
			assertTrue(false, "Should fail to create existing file without override");
		} catch (const base::Panic&) {
			// Expected behavior
		}

		// Test symlink detection
		assertTrue(!physical_file_path.isSymlink(), "Physical file should not be symlink");

		// Cleanup test directories
		fs::FileManager::deleteFolder(test_virtual_dir, true);
		fs::FileManager::deleteFolder(test_temp_dir, true);
		fs::FileManager::deleteFolder(physical_folder2, true);
		assertFalse(physical_folder2.exists(), "Physical folder should not exist after deletion");

		std::filesystem::remove_all(physical_folder_path);
		std::filesystem::remove_all(temp_folder_path);
		std::filesystem::remove(simple_physical_path);
		std::filesystem::remove(physical_file_path);
		fs::FileManager::deleteFolder(virtual_dir, true);
	}

	void fileTest() {
		// File specific tests from fileOperationsTest

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

		// Test append for virtual files
		virtual_file.writeToFile(" should not fail", true);
		auto virtual_new_content_appended = virtual_file.getContent();
		assertTrue(
			virtual_new_content_appended.view().stringView()
				== "Virtual new content should not fail",
			"Virtual content was not appended"
		);

		// Test file utilities
		auto test_file = fs::FileManager::createRandomTempFile();
		assertTrue(test_file.getFilePath().stem() != "", "File stem should not be empty");
		assertTrue(test_file.extension() == "", "File should have no extension");
		assertTrue(test_file.getFilePath().native() != "", "Absolute path should not be empty");
		assertTrue(test_file.getFilePath().uri() != "", "URI should not be empty");
		// Test getContentSafe
		auto safe_content = test_file.getContentSafe();
		ASSERT_HAS_VALUE(safe_content, "getContentSafe should succeed for existing file");

		// Delete file and test getContentSafe again
		fs::FileManager::deleteFile(test_file);
		auto safe_content_after_delete = test_file.getContentSafe();
		ASSERT_NO_VALUE(
			safe_content_after_delete, "getContentSafe should fail for non-existent file"
		);

		// Test getModifyTime for non-virtual file
		auto current_dir = std::filesystem::current_path();
		auto unique_folder_name
			= "test_physical_folder_"
		    + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
		auto physical_folder_path = current_dir / unique_folder_name;

		// Ensure cleanup from any previous run
		std::filesystem::remove_all(physical_folder_path);

		// Test symlink detection (should be false for all our test files)
		assertTrue(!test_file.getFilePath().isSymlink(), "Test files should not be symlinks");

		// Cleanup
		std::filesystem::remove_all(physical_folder_path);
	}
};

TESTER_COMMON_MAIN("/src/common/filesystem/tests/");
