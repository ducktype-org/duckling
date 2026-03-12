#pragma once

#include <frontend/pst_parser/pst.hpp>

#include <base/types/ok_bad.hpp>

#include <query_framework/external/api.hpp>

namespace lsp {
	/**
	 * @brief Register a workspace root directory. Used later by openFile to bound
	 * upward module-root search.
	 *
	 * @param absolute_physical_path Absolute path to the workspace root directory in the real file
	 * system.
	 */
	void addWorkspace(const fs::FilePath& absolute_physical_path);

	/**
	 * @brief Lazily initialise the package that owns `absolute_path`
	 * or adds the file to an already loaded package.
	 *
	 * Walks up the real filesystem from the file, stopping at the nearest
	 * registered workspace root. The topmost directory with a matching
	 * dir/dir.dmf is treated as the package root and loaded into the VFS +
	 * module tree if not already present.
	 * If the package is already loaded, the `addFile` is called.
	 *
	 * @param absolute_physical_path Absolute path to the file in the real file system.
	 */
	void openFile(const fs::FilePath& absolute_physical_path);

	/**
	 * @brief Update the content of a file in the virtual file system.
	 * It runs a query invalidation inside.
	 *
	 * If the file doesn't exist in the virtual file system, it throws an exception.
	 * @param absolute_physical_path Absolute path to the file in the real file system.
	 * @param content New content of the file.
	 */
	void updateFileContent(const fs::FilePath& absolute_physical_path, const std::string& content);

	/**
	 * @brief Add a file from real file system to the virtual file system.
	 * It also adds the file to a module or creates a new module for it if needed,
	 * and runs a query invalidation inside.
	 *
	 * @warning Adding a file only may create and append a new submodule to the module tree,
	 * but it won't add the packages that could become the submodules of the new module.
	 *
	 * @param absolute_physical_path Absolute path to the file in the real file system.
	 */
	void addFile(const fs::FilePath& absolute_physical_path);

	/**
	 * @brief Remove a file from the virtual file system.
	 * It also removes the file from a module or an entire module
	 * and runs a query invalidation inside.
	 *
	 * @param absolute_physical_path
	 */
	void removeFile(const fs::FilePath& absolute_physical_path);

	/**
	 * @brief Remove a file or a directory with all its content from the virtual file system.
	 *
	 * Calls the `removeFile` function for files, and recursively calls itself for directories.
	 *
	 * @param absolute_physical_path Absolute path to the file or directory in the real file system.
	 */
	void removeFileOrDirectory(const fs::FilePath& absolute_physical_path);

}
