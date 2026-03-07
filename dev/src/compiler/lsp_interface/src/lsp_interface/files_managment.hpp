#pragma once

#include <frontend/pst_parser/pst.hpp>

#include <base/types/ok_bad.hpp>

#include <query_framework/external/api.hpp>

namespace lsp {

	/**
	 * @brief Create a file in the virtual file system with the given content.
	 */
	void addFile(
		const fs::File& virtual_root, const std::string& path, const std::string& content
	);

	/**
	 * @brief Update the content of a file in the virtual file system.
	 * It runs a query invalidation inside.
	 *
	 * If the file doesn't exist, it creates it with empty content, updates
	 * it with the given content, and runs query invalidation.
	 *
	 * @param virtual_root The root of the virtual file system.
	 * @param path The path to the file to update, relative to the virtual root.
	 * @param content The new content of the file.
	 */
	void updateFileContent(
		const fs::File& virtual_root, const std::string& path, const std::string& content
	);

	void removeFile(const fs::File& virtual_root, const std::string& path);

	/**
	 * @brief Register a workspace root directory. Used later by openFile to bound
	 * upward module-root search.
	 */
	void addWorkspace(const fs::FilePath& absolute_physical_path);

	/**
	 * @brief Lazily initialise the package that owns `absolute_path`.
	 *
	 * Walks up the real filesystem from the file, stopping at the nearest
	 * registered workspace root. The topmost directory with a matching
	 * dir/dir.dmf is treated as the package root and loaded into the VFS +
	 * module tree if not already present.
	 */
	void openFile(const fs::FilePath& absolute_physical_path);
}
