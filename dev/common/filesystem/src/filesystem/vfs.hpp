/**
 * @file vfs.hpp
 * @author Piotr Trzaskowski (piotr.trzaskowski@outlook.com)
 */

 #pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>

//TODO
// edit a vfs class to add a
// 1. last_write_time function
// 2. function that will return std::filesystem::directory_iterator(path);
// 3. it will use a std::filesystem::path insted of std::string for handling 
// 4. use std::move for constructiong a nodepatch, that it will work or every type of system

class VFS {
public:
    VFS();
    
    // File operations
    bool createFile(const std::string& path);
    bool writeFile(const std::string& path, const std::string& content);
    std::string readFile(const std::string& path);
    
    // Directory operations
    bool createDirectory(const std::string& path);
    std::vector<std::string> listDirectory(const std::string& path);
    
    // Common operations
    bool exists(const std::string& path);
    bool isFile(const std::string& path);
    bool isDirectory(const std::string& path);

    [[nodiscard]] 
    std::string getRootPath() const { return root->name; }
    
private:
    class VFSNode {
    public:
        VFSNode(const std::string& name, bool isDir);
        
        std::string name;
        bool isDirectory;
        std::string content;  // Only used for files
        std::map<std::string, std::unique_ptr<VFSNode>> children;  // Only used for directories
    };
    
    std::unique_ptr<VFSNode> root;
    
    // Helper methods
    std::vector<std::string> splitPath(const std::string& path);
    VFSNode* findNode(const std::string& path, bool createPath = false);
    VFSNode* getParentNode(const std::string& path, bool createPath = false);
};

