#include "module_tree2.hpp"

#include <base/maps.hpp>
#include <base/string_id.hpp>
#include <query_framework/query_impl.hpp>

#include <sstream>
#include <functional>

using namespace compiler::frontend;

// Hash function for module tree state
namespace {
    u64 calculateHash(const ModuleTree2& module) {
        std::hash<std::string> hasher;
        u64 hash = 0;
        
        // Hash module ID
        hash ^= hasher(std::to_string(module.getID().asInt()));
        
        // Hash main source file if exists
        if (module.hasMainSourceFile()) {
            hash ^= hasher(module.getMainSourceFile().path.getFilePath().string());
        }
        
        // Hash source files
        for (const auto& file : module.getSourceFiles()) {
            hash ^= hasher(file.path.getFilePath().string());
        }
        
        // Hash submodules
        for (const auto& [name, submodule] : module.getSubmodules()) {
            hash ^= hasher(name.strView());
            hash ^= submodule->getHash();
        }
        
        // Hash other files
        for (const auto& [ext, files] : module.getOtherFiles()) {
            hash ^= hasher(ext.strView());
            for (const auto& file : files) {
                hash ^= hasher(file.getFilePath().string());
            }
        }
        
        return hash;
    }
}

/*********************
 * ModuleTree2 Implementation
 *********************/

ModuleTree2::ModuleTree2() : m_id(ModuleID::next()), m_hash(0), m_modified(true) {}

std::shared_ptr<ModuleTree2> ModuleTree2::create(std::shared_ptr<fs::FsTree> root) {
    auto builder = ModuleTreeBuilder::createFromFs(root);
    return builder->finalize();
}

base::Optional<base::CRef<ModuleTree2>> ModuleTree2::getParentModule() const {
    if (m_parent.has_value()) {
        CORE_ASSERT(not m_parent.value().expired(), "Parent of a module is expired!");
        return &*m_parent->lock();
    }
    return {};
}

bool ModuleTree2::hasMainSourceFile() const {
    return m_main_source_file.has_value();
}

const SourceFile& ModuleTree2::getMainSourceFile() const {
    if (!hasMainSourceFile()) {
        throw base::LogicError(base::strConcat("No main source file in module: ", getName()));
    }
    return m_main_source_file.value();
}

const std::vector<SourceFile>& ModuleTree2::getSourceFiles() const {
    return m_source_files;
}

const base::HashMap<base::StrID, std::shared_ptr<ModuleTree2>>& ModuleTree2::getSubmodules() const {
    return m_submodules;
}

const base::HashMap<base::StrID, std::vector<fs::File>>& ModuleTree2::getOtherFiles() const {
    return m_other_files;
}

base::StrID ModuleTree2::getName() const {
    if (m_fs_tree == nullptr) {
        if (hasMainSourceFile()) {
            return getMainSourceFile().lang_file_name;
        }
        return base::StrID("unknown");
    }
    return base::StrID(m_fs_tree->getRoot().name().c_str());
}

std::string ModuleTree2::prettyPrint(u32 indentation) const {
    std::stringstream output;
    
    std::string indent;
    for (u32 i = 0; i < indentation % 3; i++) indent += " ";
    for (u32 i = 0; i < indentation - (indentation % 3); i++) indent += (i % 3 == 0 ? "│" : " ");
    
    output << indent << getName().strView() << "/ [hash: " << getHash() << "]\n";
    
    if (hasMainSourceFile()) {
        output << indent << "├> " << getMainSourceFile().path.name() << '\n';
    } else {
        output << indent << "├> Missing main module file!\n";
    }
    
    for (const auto& file : getSourceFiles()) {
        output << indent << "├= " << file.path.name() << '\n';
    }
    
    for (const auto& [ext, files] : getOtherFiles()) {
        for (const auto& file : files) {
            output << indent << "├─ " << file.name() << '\n';
        }
    }
    
    for (const auto& [name, submodule] : getSubmodules()) {
        output << submodule->prettyPrint(indentation + 3);
    }
    
    return output.str();
}

ModuleID ModuleTree2::getID() const {
    return m_id;
}

u64 ModuleTree2::getUnstableHash() const {
    if (m_modified) {
        const_cast<ModuleTree2*>(this)->updateHash();
    }
    return m_hash;
}

bool ModuleTree2::isModified() const {
    return m_modified;
}

void ModuleTree2::addSourceFile(const SourceFile& file) {
    m_source_files.push_back(file);
    m_modified = true;
}

void ModuleTree2::removeSourceFile(FileID file_id) {
    auto it = std::find_if(m_source_files.begin(), m_source_files.end(),
                          [file_id](const SourceFile& f) { return f.id == file_id; });
    if (it != m_source_files.end()) {
        m_source_files.erase(it);
        m_modified = true;
    }
}

void ModuleTree2::setMainSourceFile(const SourceFile& file) {
    m_main_source_file = file;
    m_modified = true;
}

void ModuleTree2::addSubmodule(base::StrID name, std::shared_ptr<ModuleTree2> submodule) {
    m_submodules.put(name, submodule);
    submodule->m_parent = std::weak_ptr<ModuleTree2>(shared_from_this());
    m_modified = true;
}

void ModuleTree2::removeSubmodule(base::StrID name) {
    if (m_submodules.contains(name)) {
        m_submodules.erase(name);
        m_modified = true;
    }
}

void ModuleTree2::addOtherFile(const fs::File& file) {
    std::string extension = file.extension();
    base::StrID ext_id(extension.c_str());
    
    if (!m_other_files.contains(ext_id)) {
        m_other_files.put(ext_id, std::vector<fs::File>());
    }
    m_other_files[ext_id].push_back(file);
    m_modified = true;
}

void ModuleTree2::removeOtherFile(const fs::File& file) {
    std::string extension = file.extension();
    base::StrID ext_id(extension.c_str());
    
    if (m_other_files.contains(ext_id)) {
        auto& files = m_other_files[ext_id];
        auto it = std::find_if(files.begin(), files.end(),
                              [&file](const fs::File& f) { return f.getFilePath() == file.getFilePath(); });
        if (it != files.end()) {
            files.erase(it);
            m_modified = true;
        }
    }
}

void ModuleTree2::updateHash() {
    m_hash = calculateHash(*this);
    m_modified = false;
}

void ModuleTree2::buildModuleTreeFromFs(
    const std::shared_ptr<ModuleTree2>& module_root, 
    std::shared_ptr<fs::FsTree> tree_root
) {
    module_root->m_fs_tree = tree_root;
    
    // Process regular files
    for (const auto& [name, file] : tree_root->getFiles()) {
        handleNewFile(module_root, file);
    }
    
    // Process subdirectories
    for (const auto& [name, subdir] : tree_root->getDirs()) {
        auto submodule = std::make_shared<ModuleTree2>();
        buildModuleTreeFromFs(submodule, subdir);
        
        if (submodule->hasMainSourceFile()) {
            module_root->addSubmodule(base::StrID(name.c_str()), submodule);
        }
    }
}

void ModuleTree2::handleNewFile(
    const std::shared_ptr<ModuleTree2>& module_root, 
    const fs::File& filepath
) {
    std::string stem = filepath.stem();
    std::string extension = filepath.extension();
    
    if (extension == LANG_SOURCE_FILE) {
        module_root->addSourceFile(SourceFile(filepath, module_root->getID()));
    } else if (extension == LANG_MODULE_FILE) {
        base::StrID stem_id(stem.c_str());
        if (stem_id == module_root->getName()) {
            module_root->setMainSourceFile(SourceFile(filepath, module_root->getID()));
        } else {
            // Single-file module
            auto submodule = std::make_shared<ModuleTree2>();
            submodule->setMainSourceFile(SourceFile(filepath, submodule->getID()));
            module_root->addSubmodule(stem_id, submodule);
        }
    } else {
        module_root->addOtherFile(filepath);
    }
}

/*********************
 * ModuleTreeBuilder Implementation
 *********************/

ModuleTreeBuilder::ModuleTreeBuilder() : m_module(std::make_shared<ModuleTree2>()), m_finalized(false) {}

std::unique_ptr<ModuleTreeBuilder> ModuleTreeBuilder::create() {
    return std::unique_ptr<ModuleTreeBuilder>(new ModuleTreeBuilder());
}

std::unique_ptr<ModuleTreeBuilder> ModuleTreeBuilder::createFromFs(std::shared_ptr<fs::FsTree> root) {
    auto builder = create();
    ModuleTree2::buildModuleTreeFromFs(builder->m_module, root);
    return builder;
}

ModuleTreeBuilder& ModuleTreeBuilder::addSourceFile(const SourceFile& file) {
    if (m_finalized) {
        throw base::LogicError("Cannot modify finalized builder");
    }
    m_module->addSourceFile(file);
    return *this;
}

ModuleTreeBuilder& ModuleTreeBuilder::setMainSourceFile(const SourceFile& file) {
    if (m_finalized) {
        throw base::LogicError("Cannot modify finalized builder");
    }
    m_module->setMainSourceFile(file);
    return *this;
}

ModuleTreeBuilder& ModuleTreeBuilder::addSubmodule(base::StrID name, std::shared_ptr<ModuleTree2> submodule) {
    if (m_finalized) {
        throw base::LogicError("Cannot modify finalized builder");
    }
    m_module->addSubmodule(name, submodule);
    return *this;
}

ModuleTreeBuilder& ModuleTreeBuilder::addOtherFile(const fs::File& file) {
    if (m_finalized) {
        throw base::LogicError("Cannot modify finalized builder");
    }
    m_module->addOtherFile(file);
    return *this;
}

ModuleTreeBuilder& ModuleTreeBuilder::setName(base::StrID name) {
    if (m_finalized) {
        throw base::LogicError("Cannot modify finalized builder");
    }
    m_name = name;
    return *this;
}

ModuleTreeBuilder& ModuleTreeBuilder::setParent(std::shared_ptr<ModuleTree2> parent) {
    if (m_finalized) {
        throw base::LogicError("Cannot modify finalized builder");
    }
    m_module->m_parent = parent;
    return *this;
}

bool ModuleTreeBuilder::isValid() const {
    return !m_finalized && m_module != nullptr;
}

std::shared_ptr<ModuleTree2> ModuleTreeBuilder::finalize() {
    if (m_finalized) {
        throw base::LogicError("Builder already finalized");
    }
    
    m_finalized = true;
    auto result = m_module;
    m_module = nullptr;
    return result;
}
