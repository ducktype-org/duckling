/**
 * @file vfs.hpp
 * @author Piotr Trzaskowski (piotr.trzaskowski@outlook.com)
 */

#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

class VFS {
public:
	VFS();

	// File operations
	bool        createFile(const std::filesystem::path& path);
	bool        writeFile(const std::filesystem::path& path, const std::string& content);
	std::string readFile(const std::filesystem::path& path);

	// Directory operations
	bool                     createDirectory(const std::filesystem::path& path);
	std::vector<std::string> listDirectory(const std::filesystem::path& path);

	// Common operations
	bool exists(const std::filesystem::path& path);
	bool isFile(const std::filesystem::path& path);
	bool isDirectory(const std::filesystem::path& path);

	/**
	 * Checks if the path is a virtual path.
	 * @param path The path to check.
	 * @return True if the path is a virtual path, false otherwise.
	 */
	static bool isVirtualPath(const std::filesystem::path& path);

	[[nodiscard]]
	std::filesystem::path getRootPath() const {
		return root->name;
	}

private:
	class VFSNode {
	public:
		VFSNode(std::string name, bool isDir);

		std::string                                     name;
		bool                                            is_directory;
		std::string                                     content;   // Only used for files
		std::map<std::string, std::unique_ptr<VFSNode>> children;  // Only used for directories
	};

	std::unique_ptr<VFSNode> root;

	// Helper methods
	std::vector<std::string> splitPath(const std::filesystem::path& path);
	VFSNode*                 findNode(const std::filesystem::path& path, bool createPath = false);
	VFSNode* getParentNode(const std::filesystem::path& path, bool createPath = false);
};
