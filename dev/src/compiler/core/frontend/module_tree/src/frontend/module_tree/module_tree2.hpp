#pragma once

#include "file_id.hpp"
#include "module_id.hpp"
#include "source_file.hpp"

#include <filesystem/file.hpp>
#include <filesystem/fs_tree.hpp>
#include <pst_parser/pst.hpp>

#include <base/ints.hpp>
#include <base/maps.hpp>
#include <base/optional.hpp>
#include <base/ref.hpp>

#include <string>
#include <memory>

namespace compiler::frontend {
    class ModuleTreeBuilder;
    
    /**
     * ModuleTree2 - A more generic and modifiable version of ModuleTree.
     * 
     * Key improvements:
     * - Supports adding/removing files and modules
     * - Hash-based change tracking
     * - Builder pattern for construction
     * - More abstract operations
     */
    class ModuleTree2 {
        friend class ModuleTreeBuilder;
    
    public:
        /**
         * Factory method to create ModuleTree2 from filesystem tree.
         * @param root Pre-constructed std::shared_ptr<fs::FsTree> with a module structure.
         * @return A valid pointer with the root.
         */
        static std::shared_ptr<ModuleTree2> create(std::shared_ptr<fs::FsTree> root);
        
        /**
         * Accessor to module's parent module. A module might not have a parent module.
         * @return If a module has parent module, then a reference to it is passed
         * inside the base::Optional.
         */
        [[nodiscard]]
        base::Optional<base::CRef<ModuleTree2>> getParentModule() const;
        
        /**
         * Checks if a module contains main source file.
         * @return True if pointer is valid, false otherwise.
         */
        [[nodiscard]]
        bool hasMainSourceFile() const;
        
        /**
         * Accesses the main source file of the module.
         * If a pointer to file is invalid, then throws an std::logic_error exception.
         * @return A reference to the main source file.
         */
        [[nodiscard]]
        const SourceFile& getMainSourceFile() const;
        
        /**
         * Accesses the source files of the module.
         * @return A std::vector<SourceFile> with source files to iterate over.
         */
        [[nodiscard]]
        const std::vector<SourceFile>& getSourceFiles() const;
        
        /**
         * Accesses the submodules located in this submodule. Submodules are indexed by their name.
         * @return base::HashMap that maps a name of the submodule to the pointer to the submodule.
         */
        [[nodiscard]]
        const base::HashMap<base::StrID, std::shared_ptr<ModuleTree2>>& getSubmodules() const;
        
        /**
         * Accesses all the other files that are located inside the module.
         * @return A base::HashMap that maps a file extension to a vector
         * with files with this extension.
         */
        [[nodiscard]]
        const base::HashMap<base::StrID, std::vector<fs::File>>& getOtherFiles() const;
        
        /**
         * Parses the name of the module.
         * @return base::StrID with the name. `A.dmf -> A`, `/.../module/ -> module`.
         */
        [[nodiscard]]
        base::StrID getName() const;
        
        /**
         * Creates a nice, human-readable representation of this module tree.
         * @param indentation For regular printing, leave 0.
         * @return std::string with the representation.
         */
        std::string prettyPrint(u32 indentation = 0) const;
        
        /**
         * Fetches the id of the module.
         * @return compiler::frontend::ModuleID.
         */
        [[nodiscard]]
        ModuleID getID() const;
        
        /**
         * Gets the current hash of the module tree (changes with modifications).
         * @return Hash value representing current state.
         */
        [[nodiscard]]
        u64 getHash() const;
        
        /**
         * Checks if the module tree has been modified since last hash calculation.
         * @return True if modified, false otherwise.
         */
        [[nodiscard]]
        bool isModified() const;
        
    private:
        ModuleTree2();
        
        /**
         * Modification operations - private to ensure controlled access.
         */
        void addSourceFile(const SourceFile& file);
        void removeSourceFile(FileID file_id);
        void setMainSourceFile(const SourceFile& file);
        void addSubmodule(base::StrID name, std::shared_ptr<ModuleTree2> submodule);
        void removeSubmodule(base::StrID name);
        void addOtherFile(const fs::File& file);
        void removeOtherFile(const fs::File& file);
        
        /**
         * Updates the hash after modifications.
         */
        void updateHash();
        
        /**
         * Recursively builds the ModuleTree from filesystem tree.
         */
        static void buildModuleTreeFromFs(
            const std::shared_ptr<ModuleTree2>& module_root, 
            std::shared_ptr<fs::FsTree> tree_root
        );
        
        /**
         * Handles adding a new file to the module tree.
         */
        static void handleNewFile(
            const std::shared_ptr<ModuleTree2>& module_root, 
            const fs::File& filepath
        );
        
        ModuleID m_id;
        u64 m_hash;
        bool m_modified;
        
        base::Optional<std::weak_ptr<ModuleTree2>> m_parent;
        std::shared_ptr<fs::FsTree> m_fs_tree;
        
        base::Optional<SourceFile> m_main_source_file;
        std::vector<SourceFile> m_source_files;
        base::HashMap<base::StrID, std::shared_ptr<ModuleTree2>> m_submodules;
        base::HashMap<base::StrID, std::vector<fs::File>> m_other_files;
    };
    
    /**
     * ModuleTreeBuilder - Builder class for constructing ModuleTree2 instances.
     * 
     * Allows step-by-step construction of module trees with validation.
     */
    class ModuleTreeBuilder {
    public:
        /**
         * Creates a new builder instance.
         */
        static std::unique_ptr<ModuleTreeBuilder> create();
        
        /**
         * Creates a builder from existing filesystem tree.
         */
        static std::unique_ptr<ModuleTreeBuilder> createFromFs(std::shared_ptr<fs::FsTree> root);
        
        /**
         * Adds a source file to the module being built.
         */
        ModuleTreeBuilder& addSourceFile(const SourceFile& file);
        
        /**
         * Sets the main source file for the module.
         */
        ModuleTreeBuilder& setMainSourceFile(const SourceFile& file);
        
        /**
         * Adds a submodule to the module being built.
         */
        ModuleTreeBuilder& addSubmodule(base::StrID name, std::shared_ptr<ModuleTree2> submodule);
        
        /**
         * Adds an other file to the module being built.
         */
        ModuleTreeBuilder& addOtherFile(const fs::File& file);
        
        /**
         * Sets the name of the module.
         */
        ModuleTreeBuilder& setName(base::StrID name);
        
        /**
         * Sets the parent module.
         */
        ModuleTreeBuilder& setParent(std::shared_ptr<ModuleTree2> parent);
        
        /**
         * Validates the current state of the builder.
         * @return True if valid, false otherwise.
         */
        [[nodiscard]]
        bool isValid() const;
        
        /**
         * Finalizes the construction and returns the built ModuleTree2.
         * After calling this, the builder becomes invalid.
         * @return The constructed ModuleTree2.
         */
        std::shared_ptr<ModuleTree2> finalize();
        
    private:
        ModuleTreeBuilder();
        
        std::shared_ptr<ModuleTree2> m_module;
        base::StrID m_name;
        bool m_finalized;
    };
}
