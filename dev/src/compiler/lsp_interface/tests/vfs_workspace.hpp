#pragma once

#include <filesystem/file.hpp>
#include <filesystem/file_path.hpp>

#include <string>
#include <string_view>

namespace duck_ls_test {

	/**
	 * @brief Builds a source tree in the singleton VFS, standing in for the hard drive.
	 *
	 * The language server keeps its open buffers in its own VFS instance, so a tree created
	 * here plays exactly the role the physical filesystem plays in production.
	 */
	class VfsWorkspace final {
	public:
		explicit VfsWorkspace(std::string_view name):
			  root_path(std::string("vfs:/") + std::string(name)) {
			fs::FileManager::createVirtualFolder(root_path, true);
		}

		/**
		 * @brief Creates a directory below the workspace root.
		 */
		fs::FilePath addDirectory(std::string_view relative) {
			auto path = root_path.join(std::string(relative));
			fs::FileManager::createVirtualFolder(path, true);
			return path;
		}

		/**
		 * @brief Creates a file below the workspace root, with every parent already in place.
		 */
		fs::FilePath addFile(std::string_view relative, std::string_view content) {
			auto path = root_path.join(std::string(relative));
			fs::FileManager::createVirtualFile(path, content, true);
			return path;
		}

		[[nodiscard]] const fs::FilePath& root() const { return root_path; }

	private:
		fs::FilePath root_path;
	};

}
