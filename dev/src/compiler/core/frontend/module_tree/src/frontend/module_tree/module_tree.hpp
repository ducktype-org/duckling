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

namespace compiler::frontend {
	/**
	 * If a file's extension is equal to this constant, then it is assumed
	 * it is a source file of the module.
	 */
	constexpr std::string_view LANG_SOURCE_FILE = ".duck";

	/**
	 * If a file's extension is equal to this constant, then it is assumed
	 * it is a single file module.
	 */
	constexpr std::string_view LANG_MODULE_FILE = ".dmf";

    // Regexes to reject files/directories starting with '.' or '$'
    const std::regex DEFAULT_REJECT_FILE_REGEX = std::regex(R"((\$.*|\..*))");
    const std::regex DEFAULT_REJECT_DIRECTORY_REGEX = std::regex(R"((\$.*|\..*))");

    class ModuleTreeBuilder;
    class ModuleTreeModifier;

    class ModuleTree {
        friend class ModuleTreeBuilder;
        friend class ModuleTreeModifier;

        static base::StableHashMap<ModuleID, ModuleTree> module_map;
    public:

        static Ref<ModuleTree> getModule(ModuleID id);

        /**
         * Accessor to module's parent module. A module might not have a parent module.
         * @return If a module has parent module, then a reference to it is passed
         * inside the base::Optional.
         */
        [[nodiscard]]
        base::Optional<base::CRef<ModuleTree>> getParentModule() const;
        
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
        base::CRef<SourceFile> getMainSourceFile() const;

        /**
         * Accesses the source files of the module.
         * @return A std::vector<SourceFile> with source files to iterate over.
         */
        [[nodiscard]]
        const std::vector<base::Ref<SourceFile>>& getSourceFiles() const;

        /**
         * Accesses the submodules located in this submodule. Submodules are indexed by their name.
         * @return base::HashMap that maps a name of the submodule to the pointer to the submodule.
         */
        [[nodiscard]]
        const base::HashMap<base::StrID, base::Ref<ModuleTree>>& getSubmodules() const;
        
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
    private:
        ModuleTree();

        /**
         * Updates the module tree if filesystem changes are detected.
         * Similar to SourceFile::update() - checks for changes and updates IDs if needed.
         */
        void update();

        /**
         * Recursively updates parent modules.
         * This is used to ensure that changes propagate up the module tree.
         */
        void updateParentsModuleRecursively();

        /**
         * Recursively updates all submodules.
         * This is used to ensure that all submodules are in sync with the current module state.
         */
        void updateAllSubmodulesRecursively();
        
        ModuleID m_id;

        base::StrID m_name;

        base::Optional<base::Ref<ModuleTree>> m_parent;

        base::Optional<base::Ref<SourceFile>> m_main_source_file;
        std::vector<base::Ref<SourceFile>> m_source_files;
        base::HashMap<base::StrID, base::Ref<ModuleTree>> m_submodules;
        base::HashMap<base::StrID, std::vector<fs::File>> m_other_files;
    };
    
    /**
     * ModuleTreeBuilder - Builder class for constructing ModuleTree instances.
     * 
     * Allows step-by-step construction of module trees with validation.
     */
    class ModuleTreeBuilder {

    public:
        /**
         * Creates a new builder instance.
         */
        static base::Box<ModuleTreeBuilder> create();

        /**
         * Factory method to create ModuleTree from filesystem tree.
         * @param root Pre-constructed fs::File with a module structure.
         * @return A valid pointer with the root.
         */
        static Ref<ModuleTree> create(const fs::File& root, 
                                        const std::regex& file_reject = DEFAULT_REJECT_FILE_REGEX, 
                                        const std::regex& dir_reject = DEFAULT_REJECT_DIRECTORY_REGEX);

        /**
         * Adds a source file to the module being built.
         */
        bool addSourceFile(const fs::File& file);

        /**
         * Sets the main source file for the module.
         */
        bool setMainSourceFile(const fs::File& file);
        
        /**
         * Adds a submodule to the module being built.
         */
        bool addSubmodule(base::StrID name, base::Ref<ModuleTree> submodule);

        /**
         * Adds an other file to the module being built.
         */
        bool addOtherFile(const fs::File& file);
        
        /**
         * Sets the name of the module.
         */
        bool setName(base::StrID name);
        
        /**
         * Sets the parent module.
         */
        bool setParent(base::Ref<ModuleTree> parent);

        /**
         * Builds the module tree from a directory structure.
         * This will recursively traverse the directory and build the module tree.
         * @param directory The root directory to build the module tree from.
         */
        void buildFromDirectory(const fs::File& directory, 
                                const std::regex& file_reject = DEFAULT_REJECT_FILE_REGEX, 
                                const std::regex& dir_reject = DEFAULT_REJECT_DIRECTORY_REGEX);

        /**
         * Validates the current state of the builder.
         * @return True if valid, false otherwise.
         */
        [[nodiscard]]
        bool isValid() const;
        
        /**
         * Finalizes the construction and returns the built ModuleTree.
         * After calling this, the builder becomes invalid.
         * @return The constructed ModuleTree.
         */
        base::Ref<ModuleTree> finalize();

    private:
        ModuleTreeBuilder();

        void handleNewFile(const fs::File& file);

        /**
         * File/directory name validation helpers.
         */
        static bool isFileNameValid(const std::string& filename, const std::regex& reject_file_regex);
        static bool isDirectoryNameValid(const std::string& dirname, const std::regex& reject_directory_regex);

        base::Optional<base::Ref<ModuleTree>> m_parent;
        base::Optional<fs::File> m_main_source_file_path;
        std::vector<fs::File> m_source_file_paths;
        base::HashMap<base::StrID, base::Ref<ModuleTree>> m_submodules;
        base::HashMap<base::StrID, std::vector<fs::File>> m_other_files;
        
        base::StrID m_name;
        bool m_finalized;
    };

    class ModuleTreeModifier final {
    public:
        /**
         * Modification operations - private to ensure controlled access.
         */
        static bool addSourceFile(query::Context& ctx, base::Ref<ModuleTree> module, const fs::File& file);
        static bool removeSourceFile(query::Context& ctx, FileID file_id);
        static bool setMainSourceFile(query::Context& ctx, base::Ref<ModuleTree> module, const fs::File& file);
        static bool addSubmodule(query::Context& ctx, base::Ref<ModuleTree> module, base::Ref<ModuleTree> submodule);
        static bool addOtherFile(query::Context& ctx, base::Ref<ModuleTree> module, const fs::File& file);
        static bool removeOtherFile(query::Context& ctx, base::Ref<ModuleTree> module, const fs::File& file);
        static bool setParent(query::Context& ctx, base::Ref<ModuleTree> module, base::Optional<base::Ref<ModuleTree>> parent);
        static bool removeParent(query::Context& ctx, base::Ref<ModuleTree> module);

        static bool removeModule(query::Context& ctx, ModuleID module_id);

        static void fileModified(
            query::Context& ctx, const fs::File& file
        );
    private:
        ModuleTreeModifier() = default;
    };
}
