#pragma once

#include <base/maps.hpp>
#include <base/optional.hpp>
#include <base/raw_view.hpp>
#include <base/ref.hpp>

#include <expected>
#include <filesystem>
#include <string>

namespace fs {

	class FileManager;

	/**
	 * @brief Represents the type of a file.
	 *
	 * Physical: A file that exists on the physical filesystem.
	 * Virtual: A file that exists in the virtual filesystem.
	 * Temporary: A file that is temporary and managed by the system.
	 */
	enum class FileType {
		Physical,
		Virtual,
		Temporary,
	};

	/**
	 * @brief Represents the category of a file.
	 *
	 * File: A regular file.
	 * Directory: A directory.
	 */
	enum class FileCategory { File, Directory };

	/**
	 * @class File
	 * @brief Represents a file or directory in the filesystem, supporting physical, virtual,
	 * and temporary files.
	 *
	 * The File class provides a unified interface for managing and processing files and
	 * directories across three types of filesystems:
	 *
	 * 1. **Physical Filesystem**: Represents files and directories that exist on the physical disk.
	 * 2. **Virtual Filesystem**: Represents files and directories managed by a virtual filesystem
	 * (VFS).
	 * 3. **Temporary Filesystem**: Represents temporary files and directories managed by the
	 * system.
	 *
	 * ### Key Features:
	 * - **File and Directory Management**:
	 *   - Create, access, and manage files and directories in physical, virtual, and temporary
	 * filesystems.
	 *   - Support for creating unique paths for files and directories.
	 * - **Content Management**:
	 *   - Retrieve file content as `base::SharedView` objects.
	 *   - Support for safe content retrieval with error handling.
	 * - **Path Utilities**:
	 *   - Retrieve parent paths, absolute paths, URIs, and file extensions.
	 *   - Check file properties such as whether it is a directory, file, or symbolic link.
	 * - **Integration with Virtual Filesystem (VFS)**:
	 *   - Seamless handling of virtual files and directories.
	 *   - Support for reading and writing virtual file content.
	 *
	 * ### Usage:
	 * - Use `FileManager::createTempFile` and `FileManager::createRandomTempDirectory` for
	 * temporary files and directories.
	 * - Use `FileManager::createVirtualFile` and `FileManager::createRandomVirtualDirectory` for
	 * virtual files and directories.
	 * - Use `getContent` or `getContentSafe` to retrieve file content.
	 * - Use `listFilePaths` to list the contents of a directory.
	 *
	 * ### Example:
	 * ```cpp
	 * // Create a temporary file
	 * auto tempFile = FileManager::createTempFile("Temporary content");
	 *
	 * // Retrieve its content
	 * auto content = tempFile.getContent();
	 *
	 * // Create a virtual directory
	 * auto virtualDir = FileManager::createRandomVirtualDirectory();
	 *
	 * // Add a file to the virtual directory
	 * auto virtualFile = FileManager::createFileIn(virtualDir, "Virtual content");
	 * ```
	 */
	class File {
		using FileHash = std::hash<std::filesystem::path>;

		std::filesystem::path path;

		bool is_symlink;

		FileType     type;
		FileCategory category;

		friend class FileManager;
		friend struct std::hash<File>;

	public:
		File& operator=(const File&) = default;
		File& operator=(File&&)      = default;
		File(const File&)            = default;
		File(File&&)                 = default;
		~File()                      = default;

		File(const std::filesystem::path& path);

		bool operator==(const File& oth) const {
			bool are_equal = path == oth.path;
			if (are_equal) {
				CORE_ASSERT(
					type == oth.type && category == oth.category,
					"File type and category must match if paths are equal"
				);
			}
			return are_equal;
		}

		// @TODO: remove query #937
		[[nodiscard]]
		u64 queryUnstablePerfectHash() const;

		[[nodiscard]]
		base::SharedView getContent() const;
		[[nodiscard]]
		std::expected<base::SharedView, std::string> getContentSafe() const;

		[[nodiscard]]
		std::string_view strView() const;

		[[nodiscard]] std::filesystem::path getPath() const { return path; }

		[[nodiscard]]
		File parentPath() const;

		[[nodiscard]]
		std::string nativePath() const;

		[[nodiscard]]
		std::string uri() const;

		[[nodiscard]]
		std::string name() const;

		[[nodiscard]]
		FileType getType() const noexcept {
			return type;
		}

		[[nodiscard]]
		bool isFile() const noexcept;

		[[nodiscard]]
		bool isDirectory() const noexcept;

		/**
		 * @brief Returns the contents of a directory as a vector of File objects.
		 *
		 * This method works for both physical and virtual directories.
		 *
		 * @return A vector of File objects representing the contents of the directory.
		 */
		[[nodiscard]]
		std::vector<File> listFilePaths() const;

		/**
		 * @brief Checks if the file path is a symbolic link.
		 *
		 * For virtual files, this will always return false.
		 *
		 * @return True if the file path is a symbolic link, false otherwise.
		 */
		[[nodiscard]]
		bool isSymlink() const noexcept;

		/**
		 * @brief Writes content to the file.
		 *
		 * This method works for both physical and virtual files.
		 *
		 * @param new_content The content to write to the file.
		 * @param append If true, appends to the file; if false, overwrites the file.
		 * @throws CORE_PANIC if the file operation fails or if append is used with virtual files.
		 */
		void writeToFile(std::string_view new_content, bool append = false) const;

		[[nodiscard]]
		std::string stem() const;
		[[nodiscard]]
		std::string extension() const;

		bool operator<(const File& oth) const { return path < oth.path; }
	};

	/**
	 * @class FileManager
	 * @brief Provides static methods for managing files and directories.
	 *
	 * This class contains helper and factory methods for creating, managing, and generating paths
	 * for files and directories.
	 */
	class FileManager {
	public:
		/**
		 * Returns the default temporary directory as a File.
		 */
		static File getDefaultTempDirectory();

		/**
		 * Returns the default virtual directory as a File.
		 */
		static File getDefaultVirtualDirectory();

		/**
		 * Creates a new directory inside the given directory File. It is required that the
		 * directory is a temporary or virtual directory.
		 * @param directory The directory File.
		 * @param custom_name A custom name. If left default then creates a new random name.
		 * @return A File representing the newly created directory.
		 */
		static File createDirectoryIn(const File& directory, std::string_view custom_name = "");

		/**
		 * Creates a temporary directory. The directory is managed by the system and has a random
		 * name.
		 * @return A File with the new temporary directory.
		 */
		static File createRandomTempDirectory();

		/**
		 * Creates a virtual directory inside the root folder of virtual file system. The directory
		 * has a random name.
		 * @return A File with the new virtual directory.
		 */
		static File createRandomVirtualDirectory();

		/**
		 * Creates a random-named virtual file in the virtual filesystem's root directory.
		 * @param content The content to write to the file.
		 * @return The created File object.
		 */
		static File createRandomVirtualFile(std::string_view content = "");

		/**
		 * Creates a random-named temporary file in the system's temporary directory.
		 * @param content The content to write to the file.
		 * @return The created File object.
		 */
		static File createRandomTempFile(std::string_view content = "");

		/**
		 * Creates a new file inside the given directory File. Is required that the directory is a
		 * temporary or virtual directory.
		 * @param directory The directory File.
		 * @param new_file_content Content of the file to be created
		 * @param custom_name A custom name. If left default then creates a new random name.
		 * @return A File representing the newly created file.
		 */
		static File createFileIn(
			const File&      directory,
			std::string_view new_file_content,
			std::string_view custom_name = ""
		);

		/**
		 * @brief Creates a physical file in the physical filesystem's root directory or at the
		 * given absolute path. If the file already exists and override is false, throws an error.
		 * If override is true, overwrites the file.
		 * The path must be phisical.
		 * @param path The absolute or relative path to the file.
		 * @param content The content to write to the file.
		 * @param allow_overwrite If true, overwrites the file if it exists.
		 * @return The created File object.
		 */
		static File createPhysicalFile(
			const std::filesystem::path& path,
			std::string_view             content         = "",
			bool                         allow_overwrite = false
		);

		/**
		 * @brief Creates a physical folder in the physical filesystem's root directory or at the
		 * given absolute path. If the folder already exists and override is false, throws an error.
		 * If override is true, recreates the folder.
		 * The path must be phisical.
		 * @param path The absolute or relative path to the folder.
		 * @param allow_overwrite If true, recreates the folder if it exists.
		 * @return The created File object.
		 */
		static File createPhysicalFolder(
			const std::filesystem::path& path, bool allow_overwrite = false
		);

		/**
		 * @brief Creates a virtual file in the virtual filesystem's root directory or at the given
		 * path. The path must be virtual.
		 * @param path The path to the file
		 * @param content The content to write to the file.
		 * @param allow_overwrite If true, overwrites the file if it exists.
		 * @return The created File object.
		 */
		static File createVirtualFile(
			const std::filesystem::path& path,
			std::string_view             content         = "",
			bool                         allow_overwrite = false
		);

		/**
		 * @brief Creates a virtual folder in the virtual filesystem's root directory or at the
		 * given path. The path must be virtual (must start with the VFS root path: "VFS:/").
		 * @param path The path to the folder
		 * @param allow_overwrite If true, recreates the folder if it exists.
		 * @return The created File object.
		 */
		static File createVirtualFolder(
			const std::filesystem::path& path, bool allow_overwrite = false
		);

		/**
		 * @brief Creates a temporary file in the system's temporary directory or at the given path.
		 * The path must contain a temporary location.
		 * @param path The path to the file (will be placed in temp directory if not already).
		 * @param content The content to write to the file.
		 * @param allow_overwrite If true, overwrites the file if it exists.
		 * @return The created File object.
		 */
		static File createTempFile(
			const std::filesystem::path& path,
			std::string_view             content         = "",
			bool                         allow_overwrite = false
		);

		/**
		 * @brief Creates a temporary folder in the system's temporary directory or at the given
		 * path. The path must be inside system temp folder
		 * @param path The path to the folder (will be placed in temp directory if not already).
		 * @param allow_overwrite If true, recreates the folder if it exists.
		 * @return The created File object.
		 */
		static File createTempFolder(const std::filesystem::path& path, bool allow_overwrite = false);

		/**
		 * @brief Checks if a file exists at the given path (physical, virtual, or temp).
		 * @param path The path to check.
		 * @return True if the file exists, false otherwise.
		 */
		static bool fileExists(const std::filesystem::path& path);

		/**
		 * @brief Checks if a file exists for the given File object.
		 * @param file The File object to check.
		 * @return True if the file exists, false otherwise.
		 */
		static bool fileExists(const File& file);

		/**
		 * @brief Checks if a folder exists at the given path (physical, virtual, or temp).
		 * @param path The path to check.
		 * @return True if the folder exists, false otherwise.
		 */
		static bool folderExists(const std::filesystem::path& path);

		/**
		 * @brief Checks if a folder exists for the given File object.
		 * @param file The File object to check.
		 * @return True if the folder exists, false otherwise.
		 */
		static bool folderExists(const File& file);

		/**
		 * @brief Deletes a file specified by a File object, according to its type.
		 * @param file The File object to delete (const reference).
		 * @return True if the file was deleted, false otherwise.
		 */
		static bool deleteFile(const File& file);

		/**
		 * @brief Deletes a folder specified by a File object, according to its type.
		 * @param folder The File object representing the folder (const reference).
		 * @param force If true, deletes recursively. If false, only deletes empty directories.
		 * @return True if the folder was deleted, false otherwise.
		 */
		static bool deleteFolder(const File& folder, bool force = false);

		/**
		 * @brief Converts a path to a virtual path by prefixing with the VFS root path.
		 * The path must be phisical.
		 * @param path The path to convert.
		 * @return The virtual path.
		 */
		static std::filesystem::path toVirtualPath(const std::filesystem::path& path);

		/**
		 * @brief Converts a virtual path to a relative path by removing the VFS root prefix.
		 * It require that the path is virtual.
		 * @param path The virtual path.
		 * @return The path without the VFS root prefix.
		 */
		static std::filesystem::path fromVirtualPath(const std::filesystem::path& path);

		/**
		 * @brief Checks if the given path is a symbolic link.
		 *
		 * For virtual files, this will always return false.
		 *
		 * @param path The path to check.
		 * @return True if the path is a symbolic link, false otherwise.
		 */
		static bool isSymlink(const std::filesystem::path& path) noexcept;
	};
}

template<>
struct std::hash<fs::File> final {
	usize operator()(const fs::File& key) const { return fs::File::FileHash()(key.path); }
};
