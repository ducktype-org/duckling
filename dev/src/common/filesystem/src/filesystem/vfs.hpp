// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace fs {


	/**
	 * @class VFS
	 * @brief A class that implements a Virtual File System (VFS).
	 *
	 * The VFS class provides functionality to create, read, write, and manage virtual files and
	 * directories. It operates on virtual paths prefixed with "vfs:/" and abstracts the file system
	 * operations.
	 */
	class VFS {
	public:
		/**
		 * @brief Builds an empty VFS, holding nothing but its root.
		 */
		VFS();

		VFS(const VFS&)            = delete;
		VFS& operator=(const VFS&) = delete;

		/**
		 * @brief Provides access to the singleton instance of the VFS.
		 * @return A reference to the singleton VFS instance.
		 */
		static Ref<VFS> getInstance();

		/**
		 * @brief Creates a virtual file at the specified path.
		 * @param path The virtual path where the file will be created.
		 * @return True if the file was successfully created, false otherwise.
		 */
		bool createFile(const std::filesystem::path& path);

		/**
		 * @brief Writes content to a virtual file, the file must already exist.
		 * @param path The virtual path of the file.
		 * @param content The content to write to the file.
		 * @param append If true, appends the content to the file; otherwise, overwrites it.
		 * @return True if the content was successfully written, false otherwise.
		 */
		bool writeFile(
			const std::filesystem::path& path, std::string_view content, bool append = false
		);

		/**
		 * @brief Reads the content of a virtual file.
		 * @details If the file does not exist or is a directory, an exception is thrown
		 * @param path The virtual path of the file.
		 * @return The content of the file
		 */
		std::string readFile(const std::filesystem::path& path);

		/**
		 * @brief Creates a virtual directory at the specified path.
		 * @param path The virtual path where the directory will be created.
		 * @return True if the directory was successfully created, false otherwise.
		 */
		bool createDirectory(const std::filesystem::path& path);

		/**
		 * @brief Lists the contents of a virtual directory.
		 * @param path The virtual path of the directory.
		 * @return A vector of strings representing the names of the contents in the directory.
		 */
		std::vector<std::string> listDirectory(const std::filesystem::path& path);

		/**
		 * @brief Checks if a virtual path exists.
		 * @param path The virtual path to check.
		 * @return True if the path exists, false otherwise.
		 */
		bool exists(const std::filesystem::path& path);

		/**
		 * @brief Checks if a virtual path is a file.
		 * @param path The virtual path to check.
		 * @return True if the path is a file, false otherwise.
		 */
		bool isFile(const std::filesystem::path& path);

		/**
		 * @brief Checks if a virtual path is a directory.
		 * @param path The virtual path to check.
		 * @return True if the path is a directory, false otherwise.
		 */
		bool isDirectory(const std::filesystem::path& path);

		/**
		 * @brief Checks if the given path is a virtual path.
		 * @param path The path to check.
		 * @return True if the path is a virtual path, false otherwise.
		 */
		static bool isVirtualPath(const std::filesystem::path& path);

		/**
		 * @brief Gets the root path of the virtual file system.
		 * @return The root path of the VFS.
		 */
		[[nodiscard]]
		std::filesystem::path getRootPath() const {
			return root->name;
		}

		/**
		 * @brief Deletes a virtual file at the specified path.
		 * @param path The virtual path of the file to delete.
		 * @return True if the file was successfully deleted, false otherwise.
		 */
		bool deleteFile(const std::filesystem::path& path);

		/**
		 * @brief Deletes a virtual directory at the specified path.
		 * @param path The virtual path of the directory to delete.
		 * @param force If true, deletes the directory and all its contents. If false, only deletes
		 * empty directories.
		 * @return True if the directory was successfully deleted, false otherwise.
		 */
		bool deleteDirectory(const std::filesystem::path& path, bool force = false);

	private:
		class VFSNode;

		/**
		 * @brief Represents data specific to a file in the virtual file system.
		 */
		struct FileData {
			std::string content;  ///< The content of the file.
		};

		/**
		 * @brief Represents data specific to a directory in the virtual file system.
		 */
		struct DirectoryData {
			std::map<std::string, Box<VFS::VFSNode>> children;  ///< The children of the directory.
		};

		/**
		 * @class VFSNode
		 * @brief Represents a node in the virtual file system.
		 *
		 * A node can either be a file or a directory, determined by the variant data it holds.
		 */
		class VFSNode {
		public:
			/**
			 * @brief Constructs a new VFSNode object.
			 * @param name The name of the node.
			 * @param data The data associated with the node (either FileData or DirectoryData).
			 */
			VFSNode(std::string name, std::variant<FileData, DirectoryData> data);

			std::string                           name;  ///< The name of the node.
			std::variant<FileData, DirectoryData> data;  ///< The data associated with the node.

			/**
			 * @brief Checks if the node is a directory.
			 * @return True if the node is a directory, false otherwise.
			 */
			[[nodiscard]] bool isDirectory() const;

			/**
			 * @brief Checks if the node is a file.
			 * @return True if the node is a file, false otherwise.
			 */
			[[nodiscard]] bool isFile() const;
		};

		Box<VFSNode> root;  ///< The root node of the virtual file system.
		/**
		 * @brief Splits a virtual path into its components.
		 * @param path The virtual path to split.
		 * @return A vector of strings representing the components of the path.
		 */
		std::vector<std::string> splitPath(const std::filesystem::path& path);

		/**
		 * @brief Finds a node in the virtual file system.
		 * @param path The virtual path of the node.
		 * @param create_path If true, creates intermediate directories if they do not exist.
		 * @return MRef<VFSNode> A nullable reference to the node, or null if the node does not exist.
		 */
		MRef<VFSNode> findNode(const std::filesystem::path& path, bool create_path = false);

		/**
		 * @brief Gets the parent node of a given virtual path.
		 * @param path The virtual path of the node.
		 * @param create_path If true, creates intermediate directories if they do not exist.
		 * @return MRef<VFSNode> A nullable reference to the parent node, or null if the parent node
		 * does not exist.
		 */
		MRef<VFSNode> getParentNode(const std::filesystem::path& path, bool create_path = false);
	};

}
