#include "vfs.hpp"
#include <sstream>

// VFSNode implementation
VFS::VFSNode::VFSNode(const std::string& name, bool isDir) 
    : name(name), isDirectory(isDir), content("") {}

// VFS implementation
VFS::VFS() {
    root = std::make_unique<VFSNode>("/", true);
}

std::vector<std::string> VFS::splitPath(const std::string& path) {
    std::vector<std::string> parts;
    std::istringstream ss(path);
    std::string part;
    
    // Skip leading slash
    if (!path.empty() && path[0] == '/') {
        ss.ignore(1);
    }
    
    while (std::getline(ss, part, '/')) {
        if (!part.empty()) {
            parts.push_back(part);
        }
    }
    
    return parts;
}

VFS::VFSNode* VFS::findNode(const std::string& path, bool createPath) {
    if (path.empty() || path == "/") {
        return root.get();
    }
    
    auto parts = splitPath(path);
    VFSNode* current = root.get();
    
    for (size_t i = 0; i < parts.size(); ++i) {
        const auto& part = parts[i];
        auto it = current->children.find(part);
        
        if (it == current->children.end()) {
            if (createPath && i < parts.size() - 1) {
                // Create intermediate directory
                auto newNode = std::make_unique<VFSNode>(part, true);
                VFSNode* nodePtr = newNode.get();
                current->children[part] = std::move(newNode);
                current = nodePtr;
            } else if (createPath && i == parts.size() - 1) {
                // Create the final node as a file (caller will convert to directory if needed)
                auto newNode = std::make_unique<VFSNode>(part, false);
                VFSNode* nodePtr = newNode.get();
                current->children[part] = std::move(newNode);
                return nodePtr;
            } else {
                return nullptr; // Node doesn't exist and we're not creating
            }
        } else {
            current = it->second.get();
            
            // Can't navigate through a file
            if (!current->isDirectory && i < parts.size() - 1) {
                return nullptr;
            }
        }
    }
    
    return current;
}

VFS::VFSNode* VFS::getParentNode(const std::string& path, bool createPath) {
    auto parts = splitPath(path);
    if (parts.empty()) {
        return nullptr; // Root has no parent
    }
    
    // Reconstruct the path without the last part
    std::string parentPath = "/";
    for (size_t i = 0; i < parts.size() - 1; ++i) {
        parentPath += parts[i] + "/";
    }
    
    return findNode(parentPath, createPath);
}

bool VFS::createFile(const std::string& path) {
    if (path.empty() || path == "/") {
        return false; // Can't create a file at the root
    }
    
    auto parts = splitPath(path);
    std::string fileName = parts.back();
    
    // Get or create parent directory
    VFSNode* parent = getParentNode(path, true);
    if (!parent || !parent->isDirectory) {
        return false;
    }
    
    // Check if file already exists
    if (parent->children.find(fileName) != parent->children.end()) {
        return false;
    }
    
    // Create the file
    parent->children[fileName] = std::make_unique<VFSNode>(fileName, false);
    return true;
}

bool VFS::writeFile(const std::string& path, const std::string& content) {
    VFSNode* node = findNode(path);
    
    if (!node || node->isDirectory) {
        return false;
    }
    
    node->content = content;
    return true;
}

std::string VFS::readFile(const std::string& path) {
    VFSNode* node = findNode(path);
    
    if (!node || node->isDirectory) {
        return "";
    }
    
    return node->content;
}

bool VFS::createDirectory(const std::string& path) {
    if (path.empty() || path == "/") {
        return false; // Root already exists
    }
    
    auto parts = splitPath(path);
    std::string dirName = parts.back();
    
    // Get or create parent directory
    VFSNode* parent = getParentNode(path, true);
    if (!parent || !parent->isDirectory) {
        return false;
    }
    
    // Check if directory already exists
    if (parent->children.find(dirName) != parent->children.end()) {
        return false;
    }
    
    // Create the directory
    parent->children[dirName] = std::make_unique<VFSNode>(dirName, true);
    return true;
}

std::vector<std::string> VFS::listDirectory(const std::string& path) {
    VFSNode* node = findNode(path);
    
    if (!node || !node->isDirectory) {
        return {};
    }
    
    std::vector<std::string> contents;
    contents.reserve(node->children.size());
    for (const auto& pair : node->children) {
            contents.push_back(pair.first);
    }
    
    return contents;
}

bool VFS::exists(const std::string& path) {
    return findNode(path) != nullptr;
}

bool VFS::isFile(const std::string& path) {
    VFSNode* node = findNode(path);
    return node && !node->isDirectory;
}

bool VFS::isDirectory(const std::string& path) {
    VFSNode* node = findNode(path);
    return node && node->isDirectory;
}
