#include "vfs.hpp"

// VFSNode implementation
VFS::VFSNode::VFSNode(std::string name, bool isDir) 
    : name(std::move(name)), is_directory(isDir), content("") {}

// VFS implementation
VFS::VFS() {
    root = std::make_unique<VFSNode>("vroot", true);
}

std::vector<std::string> VFS::splitPath(const std::filesystem::path& path) {
    std::vector<std::string> parts;
    for (const auto& part : path) {
        if (!part.empty()) {
            parts.push_back(part.string());
        }
    }
    return parts;
}

VFS::VFSNode* VFS::findNode(const std::filesystem::path& path, bool createPath) {
    if(path.empty()) {
        return nullptr;
    }
    if (path == getRootPath()) {
        return root.get();
    }
    
    auto parts = splitPath(path);
    VFSNode* current = root.get();
    
    for (size_t i = 1; i < parts.size(); ++i) {
        const auto& part = parts[i];
        auto it = current->children.find(part);
        
        if (it == current->children.end()) {
            if (createPath) {
                // Create intermediate directory
                auto newNode = std::make_unique<VFSNode>(part, true);
                VFSNode* nodePtr = newNode.get();
                current->children[part] = std::move(newNode);
                current = nodePtr;
            } else {
                return nullptr; // Node doesn't exist and we're not creating
            }
        } else {
            current = it->second.get();
            
            // Can't navigate through a file
            if (!current->is_directory && i < parts.size() - 1) {
                return nullptr;
            }
        }
    }
    
    return current;
}

VFS::VFSNode* VFS::getParentNode(const std::filesystem::path& path, bool createPath) {
    return findNode(path.parent_path(), createPath);
}

bool VFS::createFile(const std::filesystem::path& path) {
    if (path.empty() || path == getRootPath()) {
        return false;
    }
    
    std::filesystem::path filename = path.filename();
    if (filename.empty()) {
        return false;
    }

    // Get or create parent directory
    VFSNode* parent = getParentNode(path, true);
    if (!parent || !parent->is_directory) {
        return false;
    }

    // Check if file already exists
    if (parent->children.find(filename.string()) != parent->children.end()) {
        return false;
    }
    
    // Create the file
    parent->children[filename.string()] = std::make_unique<VFSNode>(filename.string(), false);
    return true;
}

bool VFS::writeFile(const std::filesystem::path& path, const std::string& content) {
    VFSNode* node = findNode(path);
    
    if (!node || node->is_directory) {
        return false;
    }
    
    node->content = content;
    return true;
}

std::string VFS::readFile(const std::filesystem::path& path) {
    VFSNode* node = findNode(path);
    
    if (!node || node->is_directory) {
        return "";
    }
    
    return node->content;
}

bool VFS::createDirectory(const std::filesystem::path& path) {
    if (path.empty() || path == getRootPath()) {
        return false;
    }

    std::filesystem::path dirName = path.filename();
    
    // Get or create parent directory
    VFSNode* parent = getParentNode(path, true);
    if (!parent || !parent->is_directory) {
        return false;
    }
    
    // Check if directory already exists
    if (parent->children.find(dirName) != parent->children.end()) {
        return false;
    }
    
    // Create the directory
    parent->children[dirName.string()] = std::make_unique<VFSNode>(dirName.string(), true);
    return true;
}

std::vector<std::string> VFS::listDirectory(const std::filesystem::path& path) {
    VFSNode* node = findNode(path);
    
    if (!node || !node->is_directory) {
        return {};
    }
    
    std::vector<std::string> contents;
    contents.reserve(node->children.size());
    for (const auto& pair : node->children) {
            contents.push_back(pair.first);
    }
    
    return contents;
}

bool VFS::exists(const std::filesystem::path& path) {
    return findNode(path) != nullptr;
}

bool VFS::isFile(const std::filesystem::path& path) {
    VFSNode* node = findNode(path);
    return node && !node->is_directory;
}

bool VFS::isDirectory(const std::filesystem::path& path) {
    VFSNode* node = findNode(path);
    return node && node->is_directory;
}
