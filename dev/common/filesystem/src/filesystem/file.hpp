/**
 * @file file.hpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#pragma once

#include <base/maps.hpp>
#include <base/optional.hpp>
#include <base/perfect_hash.hpp>
#include <base/raw_view.hpp>

#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

// Seems fixed:
// #if __GNUC__ < 12 && (!defined(__clang__))
// // @GCC12: This specialization is in C++17, but is suported by GCC only from version 12
// // Delete this code when switched to GCC12 (currently GCC10/GCC11 is used)
// template<>
// struct std::hash<std::filesystem::path> {
// 	size_t operator()(const std::filesystem::path& path) const {
// 		return std::filesystem::hash_value(path);
// 	}
// };
// #endif

namespace fs {

	class FileContent {
		std::shared_ptr<base::OwningView> content;
		friend class FilePath;

		explicit FileContent(std::shared_ptr<base::OwningView> content):
			  content(std::move(content)) {}

	public:
		FileContent(): content(nullptr) {}

		FileContent(const FileContent&) = default;
		FileContent(FileContent&&)      = default;

		FileContent& operator=(const FileContent&) = default;
		FileContent& operator=(FileContent&&)      = default;

		usize size() { return view().size(); }

		byte operator[](usize i) { return view()[i]; }

		base::RawView view() { return content->view(); }

		[[nodiscard]]
		const base::RawView view() const {
			return content->view();
		}
	};

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
	 * @class FilePath
	 * @brief Represents a file or directory path in the filesystem, supporting physical, virtual,
	 * and temporary files.
	 *
	 * The FilePath class provides a unified interface for managing and processing files and
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
	 *   - Retrieve file content as `FileContent` objects.
	 *   - Support for safe content retrieval with error handling.
	 * - **Path Utilities**:
	 *   - Retrieve parent paths, absolute paths, URIs, and file extensions.
	 *   - Check file properties such as whether it is a directory, file, or symbolic link.
	 * - **Integration with Virtual Filesystem (VFS)**:
	 *   - Seamless handling of virtual files and directories.
	 *   - Support for reading and writing virtual file content.
	 *
	 * ### Usage:
	 * - Use `createTempFile` and `createTempDirectory` for temporary files and directories.
	 * - Use `createVirtualFile` and `createVirtualDirectory` for virtual files and directories.
	 * - Use `getContent` or `getContentSafe` to retrieve file content.
	 * - Use `listFilePaths` to list the contents of a directory.
	 *
	 * ### Example:
	 * ```cpp
	 * // Create a temporary file
	 * auto tempFile = FilePath::createTempFile("Temporary content");
	 *
	 * // Retrieve its content
	 * auto content = tempFile.getContent();
	 *
	 * // Create a virtual directory
	 * auto virtualDir = FilePath::createVirtualDirectory();
	 *
	 * // Add a file to the virtual directory
	 * auto virtualFile = virtualDir.createFileIn("Virtual content");
	 * ```
	 */
	class FilePath {
		using WeakContent = std::weak_ptr<base::OwningView>;
		using FileHash    = std::hash<std::filesystem::path>;
		using ContentMap  = base::HashMap<std::filesystem::path, WeakContent, FileHash>;

		// This might be hidden in .cpp:
		static ContentMap     to_content;
		std::filesystem::path path;

		friend struct std::hash<FilePath>;

		static FilePath getDefaultTempPath();

		static FilePath getDefaultVirtualPath();

		/**
		 * The type of the file - physical, virtual or temporary.
		 */
		FileType type = FileType::Physical;

		/**
		 * The category of the file - File or Directory.
		 */
		FileCategory category = FileCategory::File;

		/**
		 * Creates a FilePath object with the given type
		 * @param path Path of new FilePath object.
		 * @param type Type of the new FilePath object.
		 * @return A new FilePath object.
		 */
		static FilePath createFilePathObj(const std::filesystem::path& path, FileType type);

		/**
		 * A helper function creating a new unique path with a prefix of this object's path.
		 * A custom name for new filesystem file/directory. If custom_name is default, then creates
		 * a random name. It is required that this object is a temporary or virtual directory.
		 * @param custom_name A custom name for new filesystem file/directory. If left default
		 * creates a random name.
		 * @return A new, guaranteed to be unique path.
		 */
		[[nodiscard]]
		std::filesystem::path genPathInMe(std::string_view custom_name = "") const;

	public:
		FilePath& operator=(const FilePath&) = default;
		FilePath& operator=(FilePath&&)      = default;
		FilePath(const FilePath&)            = default;
		FilePath(FilePath&&)                 = default;
		~FilePath()                          = default;

		FilePath(const std::filesystem::path& path);

		bool operator==(const FilePath& oth) const {
			bool are_equal = path == oth.path;
			if (are_equal) {
				CORE_ASSERT(
					type == oth.type && category == oth.category,
					"FilePath type and category must match if paths are equal"
				);
			}
			return are_equal;
		}

		/**
		 * Creates a new directory inside this object's path. It is required that this object is a
		 * temporary or virtual directory.
		 * @param custom_name A custom name. If left default then creates a new random name.
		 * @return A path to the newly created directory.
		 */
		[[nodiscard]]
		FilePath createDirectoryIn(std::string_view custom_name = "") const;

		/**
		 * Creates a temporary directory. The directory is managed by the system and has a random
		 * name.
		 * @return A FilePath with the new temporary directory.
		 */
		static FilePath createTempDirectory();

		/**
		 * Creates a virtual directory inside the root folder of virtual file system. The directory
		 * has a random name.
		 * @return A FilePath with the new virtual directory.
		 */
		static FilePath createVirtualDirectory();


		/**
		 * Creates a new file inside this object's path. Is required that this object is a
		 * temporary or virtual directory.
		 * @param new_file_content Content of the file to be created
		 * @param custom_name A custom name. If left default then creates a new random name.
		 * @return A path to the newly created file.
		 */
		[[nodiscard]]
		FilePath createFileIn(std::string_view new_file_content, std::string_view custom_name = "")
			const;

		/**
		 * Creates a temporary file with a given content. The file is managed by the system and has
		 * a random name.
		 * @param content The content, that will be inserted into the a file.
		 * @return A FilePath with the new temporary file.
		 */
		static FilePath createTempFile(std::string_view content);

		/**
		 * @brief Creates a virtual file with a given content.
		 *
		 * The file is managed by the virtual filesystem and has a random name.
		 *
		 * @param content The content that will be inserted into the file.
		 * @return A FilePath with the new virtual file.
		 */
		static FilePath createVirtualFile(std::string_view content);

		[[nodiscard]]
		FileContent getContent() const;
		[[nodiscard]]
		std::expected<FileContent, std::string> getContentSafe() const;

		[[nodiscard]]
		std::string_view strView() const;

		[[nodiscard]]
		FilePath parentPath() const;

		[[nodiscard]]
		std::string absolutePath() const;

		[[nodiscard]]
		std::string uri() const;

		[[nodiscard]]
		std::string name() const;


		/**
		 * @brief Gets the last modification time of the file.
		 *
		 * Note: This function does not work for virtual files.
		 *
		 * @return The last modification time as a std::chrono::file_clock::time_point.
		 */
		[[nodiscard]]
		std::chrono::file_clock::time_point getModifyTime() const;

		[[nodiscard]]
		bool isFile() const noexcept;

		[[nodiscard]]
		bool isDirectory() const noexcept;

		/**
		 * @brief Returns the contents of a directory as a vector of FilePath objects.
		 *
		 * This method works for both physical and virtual directories.
		 *
		 * @return A vector of FilePath objects representing the contents of the directory.
		 */
		[[nodiscard]]
		std::vector<FilePath> listFilePaths() const;

		/**
		 * @brief Checks if the file path is a symbolic link.
		 *
		 * For virtual files, this will always return false.
		 *
		 * @return True if the file path is a symbolic link, false otherwise.
		 */
		[[nodiscard]]
		bool isSymlink() const noexcept;

		[[nodiscard]]
		std::string stem() const;
		[[nodiscard]]
		std::string extension() const;

		bool operator<(const FilePath& oth) const { return path < oth.path; }

		[[nodiscard]]
		base::HashT customPerfectHash() const;
	};

	/**
	 * @brief Reads the content of a virtual file.
	 *
	 * This function retrieves the content of a virtual file specified by its path.
	 * If the file does not exist, it throws a base::LogicError.
	 *
	 * @param path The path of the virtual file to read.
	 * @return A base::OwningView containing the content of the virtual file.
	 * @throws base::LogicError if the virtual file does not exist.
	 */
	base::OwningView getSimpleVirtualFileContent(const std::filesystem::path& path);

	/**
	 * @brief Reads the content of a non-virtual file.
	 *
	 * This function retrieves the content of a non-virtual file specified by its path.
	 * If the file does not exist, it throws a base::LogicError.
	 *
	 * @param path The path to the non-virtual file to read.
	 * @return A base::OwningView containing the content of the non-virtual file.
	 * @throws base::LogicError if the file does not exist.
	 */
	base::OwningView getSimpleFileContent(const std::filesystem::path& path);
}

template<>
struct std::hash<fs::FilePath> final {
	usize operator()(const fs::FilePath& key) const { return fs::FilePath::FileHash()(key.path); }
};
