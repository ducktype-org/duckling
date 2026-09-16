#include "vfs.hpp"

namespace fs {
	// VFSNode implementation
	VFS::VFSNode::VFSNode(std::string name, std::variant<FileData, DirectoryData> data):
		  name(std::move(name)),
		  data(std::move(data)) {}

	bool VFS::VFSNode::isDirectory() const { return std::holds_alternative<DirectoryData>(data); }

	bool VFS::VFSNode::isFile() const { return std::holds_alternative<FileData>(data); }

	// VFS implementation
	VFS::VFS(): root(makeBox<VFSNode>("vfs:", DirectoryData{ {} })) {}

	std::vector<std::string> VFS::splitPath(const std::filesystem::path& path) {
		std::vector<std::string> parts;
		for (const auto& part: path)
			if (!part.empty()) parts.push_back(part.string());
		return parts;
	}

	MRef<VFS::VFSNode> VFS::findNode(const std::filesystem::path& path, bool create_path) {
		if (path.empty()) return {};
		if (path == getRootPath()) return root.refMut();

		auto         parts   = splitPath(path);
		Ref<VFSNode> current = root.refMut();

		for (size_t i = 1; i < parts.size(); ++i) {
			const auto& part     = parts[i];
			auto&       children = std::get<DirectoryData>(current->data).children;
			auto        it       = children.find(part);

			if (it == children.end()) {
				if (create_path) {
					auto result
						= children.emplace(part, makeBox<VFSNode>(part, DirectoryData{ {} }));
					current = result.first->second.refMut();
				} else {
					return {};  // Node doesn't exist and we're not creating
				}
			} else {
				current = it->second.refMut();

				// Can't navigate through a file
				if (!current->isDirectory() && i < parts.size() - 1) return {};
			}
		}

		return current;
	}

	MRef<VFS::VFSNode> VFS::getParentNode(const std::filesystem::path& path, bool create_path) {
		return findNode(path.parent_path(), create_path);
	}

	bool VFS::createFile(const std::filesystem::path& path) {
		if (!isVirtualPath(path)) throw base::LogicError("Path is not a vfs path!");
		if (path.empty() || path == getRootPath()) return false;

		std::filesystem::path filename = path.filename();
		if (filename.empty()) return false;

		MRef<VFSNode> parent_ref = getParentNode(path, true);
		if (!parent_ref || !parent_ref->isDirectory()) return false;

		auto& children = std::get<DirectoryData>(parent_ref->data).children;
		if (children.find(filename.string()) != children.end()) return false;

		children.emplace(filename.string(), makeBox<VFSNode>(filename.string(), FileData{}));
		return true;
	}

	bool VFS::writeFile(const std::filesystem::path& path, std::string_view content, bool append) {
		if (!isVirtualPath(path)) throw base::LogicError("Path is not a vfs path!");
		MRef<VFSNode> node = findNode(path);
		if (!node || !node->isFile()) return false;

		if (append)
			std::get<FileData>(node->data).content += content;
		else
			std::get<FileData>(node->data).content = content;
		return true;
	}

	std::string VFS::readFile(const std::filesystem::path& path) {
		if (!isVirtualPath(path)) throw base::LogicError("Path is not a vfs path!");
		MRef<VFSNode> node = findNode(path);
		if (!node) CORE_PANIC(std::string("virtual file does not exist: ") + path.string());
		if (!node->isFile())
			CORE_PANIC(std::string("virtual path exists but is not a file: ") + path.string());

		return std::get<FileData>(node->data).content;
	}

	bool VFS::writeFileMetadata(const std::filesystem::path& path, std::any metadata) {
		if (!isVirtualPath(path)) throw base::LogicError("Path is not a vfs path!");
		MRef<VFSNode> node = findNode(path);
		if (!node || !node->isFile()) return false;

		std::get<FileData>(node->data).metadata = std::move(metadata);
		return true;
	}

	std::any VFS::readFileMetadata(const std::filesystem::path& path) {
		if (!isVirtualPath(path)) throw base::LogicError("Path is not a vfs path!");
		MRef<VFSNode> node = findNode(path);
		if (!node) CORE_PANIC(std::string("virtual file does not exist: ") + path.string());
		if (!node->isFile())
			CORE_PANIC(std::string("virtual path exists but is not a file: ") + path.string());

		return std::get<FileData>(node->data).metadata;
	}

	bool VFS::hasFileMetadata(const std::filesystem::path& path) {
		if (!isVirtualPath(path)) throw base::LogicError("Path is not a vfs path!");
		MRef<VFSNode> node = findNode(path);
		if (!node || !node->isFile()) return false;

		return std::get<FileData>(node->data).metadata.has_value();
	}

	bool VFS::clearFileMetadata(const std::filesystem::path& path) {
		return writeFileMetadata(path, std::any{});
	}

	bool VFS::createDirectory(const std::filesystem::path& path) {
		if (!isVirtualPath(path)) throw base::LogicError("Path is not a vfs path!");
		if (path == getRootPath()) return false;

		std::filesystem::path dir_name = path.filename();
		MRef<VFSNode>         parent   = getParentNode(path, true);
		if (!parent || !parent->isDirectory()) return false;

		auto& children = std::get<DirectoryData>(parent->data).children;
		if (children.find(dir_name.string()) != children.end()) return false;

		children.emplace(
			dir_name.string(), makeBox<VFSNode>(dir_name.string(), DirectoryData{ {} })
		);

		return true;
	}

	std::vector<std::string> VFS::listDirectory(const std::filesystem::path& path) {
		if (!isVirtualPath(path)) throw base::LogicError("Path is not a vfs path!");
		MRef<VFSNode> node = findNode(path);
		if (!node || !node->isDirectory()) return {};

		const auto&              children = std::get<DirectoryData>(node->data).children;
		std::vector<std::string> contents;
		contents.reserve(children.size());
		for (const auto& pair: children) contents.push_back(pair.first);

		return contents;
	}

	bool VFS::exists(const std::filesystem::path& path) {
		if (!isVirtualPath(path)) throw base::LogicError("Path is not a vfs path!");
		return findNode(path) != nullptr;
	}

	bool VFS::isFile(const std::filesystem::path& path) {
		if (!isVirtualPath(path)) throw base::LogicError("Path is not a vfs path!");
		MRef<VFSNode> node = findNode(path);
		return node && node->isFile();
	}

	bool VFS::isDirectory(const std::filesystem::path& path) {
		if (!isVirtualPath(path)) throw base::LogicError("Path is not a vfs path!");
		MRef<VFSNode> node = findNode(path);
		return node && node->isDirectory();
	}

	bool VFS::isVirtualPath(const std::filesystem::path& path) {
		if (path.empty()) return false;

		// Check if path starts with "vfs:"
		return *path.begin() == "vfs:";
	}

	Ref<VFS> VFS::getInstance() {
		static VFS instance;
		return &instance;
	}

	bool VFS::deleteFile(const std::filesystem::path& path) {
		if (!isVirtualPath(path)) throw base::LogicError("Path is not a vfs path!");
		MRef<VFSNode> parent_ref = getParentNode(path);
		if (!parent_ref || !parent_ref->isDirectory()) return false;

		std::filesystem::path filename = path.filename();
		if (filename.empty()) return false;

		auto& children = std::get<DirectoryData>(parent_ref->data).children;
		auto  it       = children.find(filename.string());
		if (it == children.end() || it->second->isDirectory()) return false;

		children.erase(it);
		return true;
	}

	bool VFS::deleteDirectory(const std::filesystem::path& path, bool force) {
		if (!isVirtualPath(path)) throw base::LogicError("Path is not a vfs path!");

		MRef<VFSNode> parent_ref = getParentNode(path);
		if (!parent_ref || !parent_ref->isDirectory()) return false;

		std::filesystem::path dirname = path.filename();
		if (dirname.empty()) return false;

		auto& children = std::get<DirectoryData>(parent_ref->data).children;
		auto  it       = children.find(dirname.string());
		if (it == children.end() || !it->second->isDirectory()) return false;

		if (!force) {
			// Check if the directory is empty
			const auto& dir_children = std::get<DirectoryData>(it->second->data).children;
			if (!dir_children.empty()) return false;
		}

		// Recursively delete all contents if force is true
		if (force) {
			auto& dir_children = std::get<DirectoryData>(it->second->data);
			auto  child_it     = dir_children.children.begin();
			while (child_it != dir_children.children.end()) {
				auto next_it = child_it;
				++next_it;
				if (child_it->second->isDirectory())
					deleteDirectory(path / child_it->first, true);
				else
					dir_children.children.erase(child_it);
				child_it = next_it;
			}
		}

		children.erase(it);
		return true;
	}
}
