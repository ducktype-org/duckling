#pragma once

#include <filesystem/file_path.hpp>

#include <string_view>

namespace duck_ls {

	/**
	 * @brief Everything the protocol handlers need from the compiler, behind one interface so
	 * that handler tests can record calls instead of building a module tree.
	 */
	class IFilesManagement {
	public:
		IFilesManagement()                                   = default;
		IFilesManagement(const IFilesManagement&)            = delete;
		IFilesManagement& operator=(const IFilesManagement&) = delete;
		virtual ~IFilesManagement()                          = default;

		/**
		 * @brief Registers a workspace root, bounding the upward search for a package root.
		 */
		virtual void addWorkspace(const fs::FilePath& root) = 0;

		/**
		 * @brief Makes `path` resolve to the editor's buffer instead of the file on disk.
		 */
		virtual void openDocument(const fs::FilePath& path, std::string_view text) = 0;

		/**
		 * @brief Replaces the buffer backing an already opened `path`.
		 */
		virtual void updateDocument(const fs::FilePath& path, std::string_view text) = 0;

		/**
		 * @brief Makes `path` resolve to the file on disk again.
		 */
		virtual void closeDocument(const fs::FilePath& path) = 0;

		/**
		 * @brief Reacts to a file appearing or disappearing on disk outside the editor.
		 */
		virtual void fileCreatedOrDeletedOnDisk(const fs::FilePath& path) = 0;
	};

}
