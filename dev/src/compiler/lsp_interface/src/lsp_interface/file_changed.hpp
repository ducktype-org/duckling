#pragma once

#include <frontend/pst_parser/pst.hpp>

#include <base/types/ok_bad.hpp>

#include <query_framework/external/api.hpp>

namespace lsp {
	/**
	 * @brief Create a file in the virtual file system with the given content.
	 */
	fs::File createFileFromVirtualRoot(
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

	void removeFileFromVirtualRoot(const fs::File& virtual_root, const std::string& path);
}
