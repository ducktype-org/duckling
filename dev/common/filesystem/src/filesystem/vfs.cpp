#include "vfs.hpp"

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

MRef<VFS::VFSNode> VFS::findNode(const std::filesystem::path& path, bool createPath) {
	if (path.empty()) return {};
	if (path == getRootPath()) return root.refMut();

	auto         parts   = splitPath(path);
	Ref<VFSNode> current = root.refMut();

	for (size_t i = 1; i < parts.size(); ++i) {
		const auto& part     = parts[i];
		auto&       children = std::get<DirectoryData>(current->data).children;
		auto        it       = children.find(part);

		if (it == children.end()) {
			if (createPath) {
				auto result = children.emplace(part, makeBox<VFSNode>(part, DirectoryData{ {} }));
				current     = result.first->second.refMut();
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

MRef<VFS::VFSNode> VFS::getParentNode(const std::filesystem::path& path, bool createPath) {
	return findNode(path.parent_path(), createPath);
}

bool VFS::createFile(const std::filesystem::path& path) {
	if (!isVirtualPath(path)) return false;
	if (path.empty() || path == getRootPath()) return false;

	std::filesystem::path filename = path.filename();
	if (filename.empty()) return false;

	MRef<VFSNode> parentRef = getParentNode(path, true);
	if (!parentRef || !parentRef->isDirectory()) return false;

	auto& children = std::get<DirectoryData>(parentRef->data).children;
	if (children.find(filename.string()) != children.end()) return false;

	children.emplace(filename.string(), makeBox<VFSNode>(filename.string(), FileData{ "" }));
	return true;
}

bool VFS::writeFile(const std::filesystem::path& path, const std::string& content) {
	if (!isVirtualPath(path)) return false;
	MRef<VFSNode> node = findNode(path);
	if (!node || !node->isFile()) return false;

	std::get<FileData>(node->data).content = content;
	return true;
}

std::string VFS::readFile(const std::filesystem::path& path) {
	if (!isVirtualPath(path)) return "";
	MRef<VFSNode> node = findNode(path);
	if (!node || !node->isFile()) return "";

	return std::get<FileData>(node->data).content;
}

bool VFS::createDirectory(const std::filesystem::path& path) {
	if (!isVirtualPath(path)) return false;
	if (path == getRootPath()) return false;

	std::filesystem::path dirName = path.filename();
	MRef<VFSNode>         parent  = getParentNode(path, true);
	if (!parent || !parent->isDirectory()) return false;

	auto& children = std::get<DirectoryData>(parent->data).children;
	if (children.find(dirName.string()) != children.end()) return false;

	children.emplace(dirName.string(), makeBox<VFSNode>(dirName.string(), DirectoryData{ {} }));

	return true;
}

std::vector<std::string> VFS::listDirectory(const std::filesystem::path& path) {
	if (!isVirtualPath(path)) return {};
	MRef<VFSNode> node = findNode(path);
	if (!node || !node->isDirectory()) return {};

	const auto&              children = std::get<DirectoryData>(node->data).children;
	std::vector<std::string> contents;
	contents.reserve(children.size());
	for (const auto& pair: children) contents.push_back(pair.first);

	return contents;
}

bool VFS::exists(const std::filesystem::path& path) {
	if (!isVirtualPath(path)) return false;
	return findNode(path) != nullptr;
}

bool VFS::isFile(const std::filesystem::path& path) {
	if (!isVirtualPath(path)) return false;
	MRef<VFSNode> node = findNode(path);
	return node && node->isFile();
}

bool VFS::isDirectory(const std::filesystem::path& path) {
	if (!isVirtualPath(path)) return false;
	MRef<VFSNode> node = findNode(path);
	return node && node->isDirectory();
}

bool VFS::isVirtualPath(const std::filesystem::path& path) {
	if (path.empty()) return false;
	return *path.begin() == "vfs:";
}
