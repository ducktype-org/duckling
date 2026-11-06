// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "file_path.hpp"

#include <base/collections/optional.hpp>
#include <base/misc/shared_view.hpp>
#include <base/types/expected_utils.hpp>

#include <expected>
#include <filesystem>
#include <functional>
#include <ostream>
#include <string>

namespace fs {

	class FileManager;

	/**
	 * @brief Represents the type of a file.
	 *
	 * Physical: A file that exists on the physical filesystem.
	 * Virtual: A file that exists in the virtual filesystem.
	 */
	enum class FileType {
		Physical,
		Virtual,
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
	 * @brief Represents a file or directory in the filesystem, supporting physical (including
	 * temp-dir) and virtual files.
	 *
	 * The File class provides a unified interface for managing and processing files and
	 * directories across two types of filesystems:
	 *
	 * 1. **Physical Filesystem**: Represents files and directories that exist on the physical
	 * disk. The system's temporary directory is part of it, so a temporary file is a physical
	 * one that additionally answers `isTemporary()`.
	 * 2. **Virtual Filesystem**: Represents files and directories managed by a virtual filesystem
	 * (VFS).
	 *
	 * ### Key Features:
	 * - **File and Directory Management**:
	 *   - Create, access, and manage files and directories in the physical and virtual
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
	 * - Use `FileManager::createRandomTempFile` and `FileManager::createRandomTempDirectory` for
	 * temporary files and directories. A file at a known path in the temp directory is created
	 * with the physical factories - the temp directory is part of the physical filesystem.
	 * - Use `FileManager::createVirtualFile` and `FileManager::createRandomVirtualDirectory` for
	 * virtual files and directories.
	 * - Use `getContent` or `getContentSafe` to retrieve file content.
	 * - Use `listFilePaths` to list the contents of a directory.
	 *
	 * ### Example:
	 * ```cpp
	 * // Create a temporary file
	 * auto tempFile = FileManager::createRandomTempFile("Temporary content");
	 *
	 * // Retrieve its content
	 * auto content = tempFile.getContent();
	 *
	 * // Create a virtual directory
	 * auto virtualDir = FileManager::createRandomVirtualDirectory();
	 *
	 * // Add a file to the virtual directory
	 * auto virtualFile = virtualDir.createSubFile("Virtual content");
	 * ```
	 */
	class File final {
		using FileHash = std::hash<std::filesystem::path>;

		FilePath path;

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

		/**
		 * @brief Makes a `File` for an existing file or directory.
		 * @note Panics if nothing exists at `path`; use `open` where it may be missing.
		 */
		File(const FilePath& path);

		/**
		 * @brief Makes a `File` for an existing file or directory, reporting failure instead of
		 * panicking.
		 * @return The file, or a message describing why it could not be opened (e.g. it does
		 * not exist).
		 */
		[[nodiscard]]
		static std::expected<File, std::string> open(const FilePath& path);

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

		/**
		 * @brief Reads the whole file.
		 * @return The content, or a message describing why it could not be read.
		 */
		[[nodiscard]]
		std::expected<base::SharedView, std::string> getContent() const;

		[[nodiscard]] FilePath getFilePath() const { return path; }

		[[nodiscard]]
		std::string name() const;

		[[nodiscard]]
		std::string stem() const;

		[[nodiscard]]
		FileType getType() const noexcept {
			return type;
		}

		/**
		 * @brief Checks whether the file lives in the system's temporary directory.
		 * @return True if the file lies in the system temp directory, false otherwise.
		 */
		[[nodiscard]]
		bool isTemporary() const;

		[[nodiscard]]
		bool isFile() const noexcept;

		[[nodiscard]]
		bool isDirectory() const noexcept;

		/**
		 * @brief Writes content to the file, reporting failure instead of panicking.
		 *
		 * A write that fails usually says something about the machine - no space left, a
		 * read-only mount - and that is for the user to hear, not for the compiler to abort on.
		 *
		 * This method works for both file types (physical and virtual).
		 *
		 * @param new_content The content to write to the file.
		 * @param append If true, appends to the file; if false, overwrites the file.
		 * @return Nothing on success, or a message describing why the write failed.
		 */
		[[nodiscard]]
		std::expected<void, std::string> writeToFile(
			std::string_view new_content, bool append = false
		) const;

		/**
		 * @brief Returns the contents of a directory as a vector of FilePath objects.
		 *
		 * This method works for both physical and virtual directories.
		 *
		 * @return The paths of the directory's entries, or a message describing why they could not
		 * be listed (e.g. this is not a directory).
		 */
		[[nodiscard]]
		std::expected<std::vector<FilePath>, std::string> listFilePaths() const;

		/**
		 * @brief Creates a subdirectory inside this directory.
		 *
		 * This method works for all file types (physical, virtual, temporary).
		 *
		 * @param custom_name A custom name. If left default then creates a new random name.
		 * @return The new subdirectory, or a message describing why it could not be created (e.g.
		 * the name is taken).
		 */
		[[nodiscard]]
		std::expected<File, std::string> createSubDirectory(std::string_view custom_name = "") const;

		/**
		 * @brief Creates a file inside this directory.
		 *
		 * This method works for virtual and temporary directories.
		 *
		 * @param new_file_content Content of the file to be created
		 * @param custom_name A custom name. If left default then creates a new random name.
		 * @return The new file, or a message describing why it could not be created (e.g. the
		 * name is taken).
		 */
		[[nodiscard]]
		std::expected<File, std::string> createSubFile(
			std::string_view new_file_content, std::string_view custom_name = ""
		) const;

		/**
		 * @brief Checks if this file or directory exists.
		 *
		 * This method works for all file types (physical, virtual, temporary).
		 *
		 * @return True if the file or directory exists, false otherwise.
		 */
		[[nodiscard]]
		bool exists() const;

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
	 *
	 * Every operation that touches the disk reports failure - a missing file, no permission, a
	 * full disk - in its result instead of throwing or panicking; the caller decides what a
	 * failure means. Where it really is a broken invariant, `base::valueOrPanic` gives up on it
	 * visibly. Only misuse - e.g. a virtual path where a physical one is required - panics.
	 * Virtual files live in memory, so creating them cannot fail for such reasons.
	 */
	class FileManager final {
	public:
		/**
		 * @brief Picks a path `<directory>/<prefix><random><suffix>` that does not exist yet.
		 *
		 * Only the name is picked - nothing is created - so another process can still take the
		 * same path before the caller does. The random part is long enough that this does not
		 * happen in practice; it is meant for names that only need to differ between concurrent
		 * writers, such as a temporary file renamed into place afterwards.
		 * @param directory The directory the path is placed in.
		 * @param prefix Text put in front of the random part.
		 * @param suffix Text put after the random part, e.g. ".tmp".
		 * @return The chosen path.
		 */
		static FilePath randomName(
			const FilePath& directory, std::string_view prefix = "", std::string_view suffix = ""
		);

		/**
		 * @brief Length of the random part `randomName` puts in a name - always exactly this
		 * many letters and digits. Code that recognises such names, e.g. the temporary files of
		 * `produceFileAtomically` left by a killed process, can rely on it.
		 */
		static constexpr std::size_t RANDOM_NAME_LENGTH = 16;

		/**
		 * Creates a temporary directory. The directory is managed by the system and has a random
		 * name.
		 *
		 * The system temp directory is part of the physical filesystem, so a file placed there is
		 * created with the physical factories like any other. Only the *random naming* is specific
		 * to temporary files, which is all this helper and `createRandomTempFile` add.
		 * @return The new temporary directory, or a message describing why it could not be made.
		 */
		[[nodiscard]]
		static std::expected<File, std::string> createRandomTempDirectory();

		/**
		 * Creates a virtual directory inside the root folder of virtual file system. The directory
		 * has a random name.
		 * @param vfs The virtual filesystem to create the directory in.
		 * @return A File with the new virtual directory.
		 */
		static File createRandomVirtualDirectory(base::Ref<VFS> vfs = VFS::getInstance());

		/**
		 * Creates a random-named virtual file in the virtual filesystem's root directory.
		 * @param content The content to write to the file.
		 * @param suffix Optional suffix to append to the filename (e.g., ".dk").
		 * @param vfs The virtual filesystem to create the file in.
		 * @return The created File object.
		 */
		static File createRandomVirtualFile(
			std::string_view                 content = "",
			base::Optional<std::string_view> suffix  = {},
			base::Ref<VFS>                   vfs     = VFS::getInstance()
		);

		/**
		 * Creates a random-named temporary file in the system's temporary directory.
		 * Like `createRandomTempDirectory`, it only picks a random name and then creates a
		 * physical file there.
		 * @param content The content to write to the file.
		 * @return The new temporary file, or a message describing why it could not be made.
		 */
		[[nodiscard]]
		static std::expected<File, std::string> createRandomTempFile(std::string_view content = "");

		/**
		 * @brief Creates a physical file, reporting failure instead of panicking.
		 *
		 * Creating a file can fail for reasons that have nothing to do with the caller being
		 * wrong: no write permission, a read-only mount, a full disk.
		 * The path must be absolute and on disk; a path under the system temp directory counts
		 * (physical and temporary are the same filesystem), a virtual one does not.
		 * @param path The absolute on-disk path to the file.
		 * @param content The content to write to the file.
		 * @param allow_overwrite If true, overwrites the file if it exists.
		 * @return The created File, or a message describing why it could not be created.
		 */
		[[nodiscard]]
		static std::expected<File, std::string> createPhysicalFile(
			const FilePath& path, std::string_view content = "", bool allow_overwrite = false
		);

		/**
		 * @brief Creates a physical folder, reporting failure instead of panicking.
		 *
		 * A folder that cannot be created is usually a fact about the machine, not a bug in the
		 * caller.
		 *
		 * @param path The absolute on-disk path to the folder.
		 * @param allow_overwrite If true, recreates the folder if it exists.
		 * @return The created File, or a message describing why it could not be created.
		 */
		[[nodiscard]]
		static std::expected<File, std::string> createPhysicalFolder(
			const FilePath& path, bool allow_overwrite = false
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
			const FilePath& path, std::string_view content = "", bool allow_overwrite = false
		);

		/**
		 * @brief Creates a virtual folder in the virtual filesystem's root directory or at the
		 * given path. The path must be virtual (must start with the VFS root path: "VFS:/").
		 * @param path The path to the folder
		 * @param allow_overwrite If true, recreates the folder if it exists.
		 * @return The created File object.
		 */
		static File createVirtualFolder(const FilePath& path, bool allow_overwrite = false);

		/**
		 * @brief Deletes a file specified by a File object, according to its type.
		 * @param file The File object to delete (const reference).
		 * @return True if the file was deleted, false if it was already gone, or a message
		 * describing why it could not be deleted.
		 */
		[[nodiscard]]
		static std::expected<bool, std::string> deleteFile(const File& file);

		/**
		 * @brief Deletes a folder specified by a File object, according to its type.
		 * @param folder The File object representing the folder (const reference).
		 * @param force If true, deletes recursively. If false, only deletes empty directories.
		 * @return True if the folder was deleted, false if it was already gone, or a message
		 * describing why it could not be deleted (e.g. it is not empty).
		 */
		[[nodiscard]]
		static std::expected<bool, std::string> deleteFolder(const File& folder, bool force = false);

		/**
		 * @brief If `path` is a temporary file `produceFileAtomically` makes - named
		 * `<target>.<random>.tmp` - returns the path of its `<target>`, next to it.
		 *
		 * The random part is matched exactly (`RANDOM_NAME_LENGTH` letters and digits), so a
		 * user's `app.backup.tmp` next to an `app` is not taken for one.
		 */
		[[nodiscard]]
		static base::Optional<FilePath> temporaryFileTarget(const FilePath& path);

		/**
		 * @brief Creates the file at a given path, e.g. by writing it or by running a tool that
		 * writes it. Returns a message describing the failure, if it failed.
		 */
		using FileProducer
			= std::function<std::expected<void, std::string>(const FilePath& temp_path)>;

		/**
		 * @brief Has `produce` create a physical file at a temporary path next to `path`, then
		 * renames it into place, reporting failure instead of panicking.
		 *
		 * A rename inside one directory is atomic, so a reader sees either the previous file or
		 * the new one, never a half of each, and a producer that fails or dies partway (a full
		 * disk, a killed process, a failing linker) leaves the previous file untouched. Each call
		 * gets its own temporary path, named `<file name>.<random>.tmp`, so concurrent producers
		 * cannot mix their bytes either: the last rename wins, whole. A process killed midway can
		 * leave that temporary file behind; nothing else does.
		 *
		 * @param path The file to produce; it must be on disk (absolute or relative), not
		 * virtual. It does not have to exist yet.
		 * @param produce Creates the file at the temporary path it is given.
		 * @return Nothing on success. Otherwise `produce`'s own failure, or a message describing
		 * why the produced file could not be put in place.
		 */
		[[nodiscard]]
		static std::expected<void, std::string> produceFileAtomically(
			const FilePath& path, const FileProducer& produce
		);

		/**
		 * @brief `produceFileAtomically` for a file written through a stream.
		 *
		 * @param path The file to write; it must be on disk (absolute or relative), not virtual.
		 * It does not have to exist yet.
		 * @param write_content Writes the file's content to the given stream.
		 * @return Nothing on success, or a message describing why the write failed.
		 */
		[[nodiscard]]
		static std::expected<void, std::string> writeFileAtomically(
			const FilePath& path, const std::function<void(std::ostream&)>& write_content
		);

		/**
		 * @brief Removes a physical file, symlink or empty folder, reporting failure instead of
		 * throwing.
		 *
		 * Takes a `FilePath`, not a `File`, on purpose: a `File` resolves symlinks when it is
		 * made, so removing one would remove the link's target. This removes the link itself.
		 *
		 * @param path The entry to remove; it must be on disk (absolute or relative), not virtual.
		 * @return True if the entry was removed, false if there was nothing to remove, or a
		 * message describing why it could not be removed (e.g. a folder that is not empty).
		 */
		[[nodiscard]]
		static std::expected<bool, std::string> removeEntry(const FilePath& path);

		/**
		 * @brief Sets a physical file's modification time to now, like the `touch` command,
		 * reporting failure instead of throwing.
		 *
		 * Like `touch`, it needs only write permission to the file, not its ownership - so it
		 * works on a file another user created in a folder they share. Unlike `touch`, it never
		 * creates the file: a missing file is reported as a failure.
		 *
		 * @param path The file to touch; it must be on disk (absolute or relative), not virtual.
		 * @return Nothing on success, or a message describing why the time could not be set.
		 */
		[[nodiscard]]
		static std::expected<void, std::string> touch(const FilePath& path);
	};
}

template<>
struct std::hash<fs::File> final {
	usize operator()(const fs::File& key) const { return fs::File::FileHash()(key.path); }
};
