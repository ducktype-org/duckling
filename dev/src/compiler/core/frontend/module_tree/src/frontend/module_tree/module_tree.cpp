#include "module_tree.hpp"
#include "queries.hpp"

#include <algorithm>
#include <base/exceptions.hpp>
#include <base/string_id.hpp>
#include <query_framework/query_impl.hpp>

#include <sstream>
#include <regex>

#ifdef DEBUG
#include <unordered_set>
#endif

namespace {
#ifdef DEBUG
    std::unordered_set<compiler::frontend::ModuleID> changed_modules_ids;
#endif

    /**
    * @brief Map storting FileID of each parsed PST (by root element ID)
    * @note: as of right not it is needed only for QueryPrimaryCodeScopeFor for acquiring
    * the root scope via extendQueryModuleIDOfPST.
    * @todo: Either delete root scopes and add to PST some kind of "module nodes" or put information
    * from this map into PST nodes.
    */
    inline static base::Map<pst::PstID, compiler::frontend::FileID> root_element_file_back_map;
}

namespace compiler::frontend {
    Ref<ModuleTree> ModuleTree::getModule(ModuleID id){
#ifdef DEBUG
		CORE_ASSERT(
			!changed_modules_ids.contains(id),
			"Module with ID " + std::to_string(id.asInt()) + " has changed since last query. Please use the new ModuleID."
		);
#endif
        CORE_ASSERT(module_map.contains(id), "Module with ID " + std::to_string(id.asInt()) + " does not exist!");
        return module_map.atMaybe(id).value();
    }

    Ref<ModuleTree> ModuleTreeBuilder::create(const fs::File& root, const std::regex& file_reject, const std::regex& dir_reject) {
        base::Box<ModuleTreeBuilder> builder = ModuleTreeBuilder::create();

        builder->buildFromDirectory(root, file_reject, dir_reject);

        return builder->finalize();
    }

    ModuleTree::ModuleTree() : m_id(ModuleID::next()) {
        module_map.put(m_id, *this);
    }

    base::Optional<base::CRef<ModuleTree>> ModuleTree::getParentModule() const {
        if (m_parent.has_value()) {
            return m_parent.value();
        }
        return {};
    }

    bool ModuleTree::hasMainSourceFile() const {
        return m_main_source_file.has_value();
    }

    base::CRef<SourceFile> ModuleTree::getMainSourceFile() const {
        return m_main_source_file.value();
    }

    const std::vector<base::Ref<SourceFile>>& ModuleTree::getSourceFiles() const {
        return m_source_files;
    }

    const base::HashMap<base::StrID, base::Ref<ModuleTree>>& ModuleTree::getSubmodules() const {
        return m_submodules;
    }

    const base::HashMap<base::StrID, std::vector<fs::File>>& ModuleTree::getOtherFiles() const {
        return m_other_files;
    }

    base::StrID ModuleTree::getName() const {
        return m_name;
    }

    std::string ModuleTree::prettyPrint(u32 indentation) const {
        std::stringstream output;
        
        std::string indent;
        for (u32 i = 0; i < indentation % 3; i++) indent += " ";
        for (u32 i = 0; i < indentation - (indentation % 3); i++) indent += (i % 3 == 0 ? "│" : " ");
        
        output << indent << getName().strView() << "/ [id: " << getID().asInt() << "]\n";
        
        if (hasMainSourceFile()) {
            output << indent << "├> " << getMainSourceFile()->getPath().name() << '\n';
        } else {
            output << indent << "├> Missing main module file!\n";
        }
        
        for (const auto& file_ref : getSourceFiles()) {
            output << indent << "├= " << file_ref->getPath().name() << '\n';
        }
        
        for (const auto& [ext, files] : getOtherFiles()) {
            for (const auto& file : files) {
                output << indent << "├─ " << file.name() << '\n';
            }
        }
        
        for (const auto& [name, submodule_ref] : getSubmodules()) {
            output << submodule_ref->prettyPrint(indentation + 3);
        }
        
        return output.str();
    }

    ModuleID ModuleTree::getID() const {
        return m_id;
    }

    bool ModuleTreeBuilder::isFileNameValid(const std::string& filename, 
                                            const std::regex& reject_file_regex) {
        std::smatch match;
        return !std::regex_match(filename, match, reject_file_regex);
    }

    bool ModuleTreeBuilder::isDirectoryNameValid(const std::string& dirname, 
                                                 const std::regex& reject_directory_regex) {
        std::smatch match;
        return !std::regex_match(dirname, match, reject_directory_regex);
    }

    void ModuleTreeBuilder::buildFromDirectory(const fs::File& directory, 
                                                const std::regex& file_reject, 
                                                const std::regex& dir_reject) {
        if (!directory.isDirectory()) {
            throw base::LogicError("Expected directory, got file: " + directory.getFilePath().string());
        }

        // Process all files and directories in the current directory
        for (const auto& path : directory.listFilePaths()) {
            // Skip symlinks to avoid cycles
            if (path.isSymlink()) continue;

            fs::File file(path);

            if (file.isDirectory()) {
                // Handle subdirectory
                if (!isDirectoryNameValid(file.name(), dir_reject)) continue;

                // Create submodule for this directory
                auto subbuilder = ModuleTreeBuilder::create();
                subbuilder->buildFromDirectory(file);
                auto submodule = subbuilder->finalize();

                // Only add submodules that have a main source file
                if (submodule->hasMainSourceFile()) {
                    base::StrID dir_name(file.name().c_str());
                    addSubmodule(dir_name, submodule);
                }
            } else {
                // Handle regular file
                if (!isFileNameValid(file.name(), file_reject)) continue;
                handleNewFile(file);
            }
        }
    }

    void ModuleTreeBuilder::handleNewFile(const fs::File& file) {
        std::string stem = file.stem();
        std::string extension = file.extension();

        if (extension == LANG_SOURCE_FILE) {
            // Regular source file - store path for later
            addSourceFile(file);
        } else if (extension == LANG_MODULE_FILE) {
            // Module file - store path for later
            base::StrID stem_id(stem.c_str());
            
            setMainSourceFile(file);
            setName(stem_id);
        } else {
            // Other file
            addOtherFile(file);
        }
    }

    void ModuleTree::updateParentsModuleRecursively() {
            // We need to update the parent module as well
            if (m_parent.has_value()) {
                m_parent.value()->update();
                m_parent.value()->updateParentsModuleRecursively();
            }
    }

    void ModuleTree::update(){
#ifdef DEBUG
            changed_modules_ids.insert(m_id);
#endif
            // Remove old module from map
            module_map.erase(m_id);

            m_id = ModuleID::next();
            // Re-add to map with new ID
            module_map.put(m_id, *this);
    }

    void ModuleTree::updateAllSubmodulesRecursively() {

        // Update all source files in this module
        for (auto& file : m_source_files) {
            file->update();
        }

        // Update main source file if it exists
        if (m_main_source_file.has_value()) {
            m_main_source_file.value()->update();
        }

        // Update all submodules recursively
        for (auto& [_, submodule] : m_submodules) {
            submodule->update();
            submodule->updateAllSubmodulesRecursively();
        }
    }

    /*********************
     * ModuleTreeBuilder Implementation
     *********************/

    ModuleTreeBuilder::ModuleTreeBuilder() : m_finalized(false) {}

    base::Box<ModuleTreeBuilder> ModuleTreeBuilder::create() {
        return base::makeBox<ModuleTreeBuilder>();
    }

    bool ModuleTreeBuilder::addSourceFile(const fs::File& file) {
        CORE_ASSERT(!m_finalized, "Builder already finalized");
        m_source_file_paths.push_back(file);
        return true;
    }

    bool ModuleTreeBuilder::setMainSourceFile(const fs::File& file) {
        CORE_ASSERT(!m_finalized, "Builder already finalized");
        CORE_ASSERT(!m_main_source_file_path.has_value(), "Main source file already set");
        m_main_source_file_path = file;
        return true;
    }

    bool ModuleTreeBuilder::addSubmodule(base::StrID name, base::Ref<ModuleTree> submodule) {
        CORE_ASSERT(!m_finalized, "Builder already finalized");
        CORE_ASSERT(!m_submodules.contains(name), "Submodule with the same name already added");
        m_submodules.put(name, submodule);
        return true;
    }

    bool ModuleTreeBuilder::addOtherFile(const fs::File& file) {
        CORE_ASSERT(!m_finalized, "Builder already finalized");
        std::string extension = file.extension();
        base::StrID ext_id(extension.c_str());
        
        if (!m_other_files.contains(ext_id)) {
            m_other_files.put(ext_id, std::vector<fs::File>());
        }
        m_other_files.at(ext_id).push_back(file);
        return true;
    }

    bool ModuleTreeBuilder::setName(base::StrID name) {
        CORE_ASSERT(!m_finalized, "Builder already finalized");
        CORE_ASSERT(m_name.isBad(), "Module name is already set");
        m_name = name;
        return true;
    }

    bool ModuleTreeBuilder::setParent(base::Ref<ModuleTree> parent) {
        CORE_ASSERT(!m_finalized, "Builder already finalized");
        m_parent = parent;
        return true;
    }

    bool ModuleTreeBuilder::isValid() const {
        return !m_finalized;
    }

    base::Ref<ModuleTree> ModuleTreeBuilder::finalize() {
        CORE_ASSERT(!m_finalized, "Builder already finalized");

        m_finalized = true;
        
        // Create new ModuleTree instance
        auto module = new ModuleTree();
        auto module_ref = Ref<ModuleTree>(module);
        
        // Set ID and name
        module->m_name = m_name;
        // Create SourceFiles from stored paths
        if (m_main_source_file_path.has_value()) {
            module->m_main_source_file = SourceFile::create(m_main_source_file_path.value(), module_ref);
        }
        
        for (const auto& file_path : m_source_file_paths) {
            auto source_file = SourceFile::create(file_path, module_ref);
            module->m_source_files.push_back(source_file);
        }
        
        // Transfer other data from builder to module
        module->m_submodules = std::move(m_submodules);
        module->m_other_files = std::move(m_other_files);
        module->m_parent = m_parent;
        
        // Set parent references for submodules
        for (const auto& [name, submodule] : module->m_submodules) {
            submodule->m_parent = module_ref;
        }
        
        return module_ref;
    }

    /*********************
     * ModuleTreeModifier Implementation
     *********************/

    bool ModuleTreeModifier::addSourceFile(query::Context& ctx, base::Ref<ModuleTree> module, const fs::File& file) {
        module->m_source_files.push_back(SourceFile::create(file, module));
        module->update();
        module->updateParentsModuleRecursively();
        return true;
    }

    bool ModuleTreeModifier::removeSourceFile(query::Context& ctx, FileID file_id) {

        Ref<SourceFile> file = SourceFile::getSourceFile(file_id);
        CRef<ModuleTree> linked_module = file->getModule();

        Ref<ModuleTree> module = ModuleTree::module_map.atMaybe(linked_module->getID()).value();
        
        auto& source_files = module->m_source_files;
        auto it = std::ranges::find_if(source_files,
                              [file_id](const base::Ref<SourceFile>& file) {
                                  return file->getID() == file_id;
                              });

        CORE_ASSERT(it != source_files.end(), "SourceFile not found in module");

        SourceFile::file_map.erase(file_id);
        source_files.erase(it);
        module->update();
        module->updateParentsModuleRecursively();
        return true;
    }

    bool ModuleTreeModifier::setMainSourceFile(query::Context& ctx, base::Ref<ModuleTree> module, const fs::File& file) {
        CORE_ASSERT(!module->m_main_source_file.has_value(), "Main source file is already set, remove it first");
        module->m_main_source_file = SourceFile::create(file, module);
        module->update();
        module->updateParentsModuleRecursively();
        return true;
    }

    bool ModuleTreeModifier::addSubmodule(query::Context& ctx, base::Ref<ModuleTree> module, base::Ref<ModuleTree> submodule) {
        base::StrID name = submodule->getName();
        CORE_ASSERT(!module->m_submodules.contains(name), base::strConcat(
            "Submodule with name '", name.strView(), "' already exists in module ", module->getName().strView()
            , " call remove first!"
        ));
        
        module->m_submodules.put(name, submodule);

        if(submodule->m_parent.has_value()) {
            removeParent(ctx, submodule);
        } else {
            //We need to update all submodules recursively this is because adding a parent will change all mangled names
            submodule->update();
            submodule->updateAllSubmodulesRecursively();
        }
        submodule->m_parent = module;
        
        module->update();
        module->updateParentsModuleRecursively();

        return true;
    }

    bool ModuleTreeModifier::addOtherFile(query::Context& ctx, base::Ref<ModuleTree> module, const fs::File& file) {
        std::string extension = file.extension();
        base::StrID ext_id(extension.c_str());
        
        if (!module->m_other_files.contains(ext_id)) {
            module->m_other_files.put(ext_id, std::vector<fs::File>());
        }
        
        CORE_ASSERT(!std::ranges::any_of(module->m_other_files.at(ext_id), [&file](const fs::File& f) {
            return f.getFilePath() == file.getFilePath();
        }), base::strConcat(
            "Other file with path '", file.getFilePath().string(), "' already exists in module ", module->getName().strView()
        ));

        module->m_other_files.at(ext_id).push_back(file);
        //@TODO: do we need to update the module here?
        //module->update();
        return true;
    }

    bool ModuleTreeModifier::removeOtherFile(query::Context& ctx, base::Ref<ModuleTree> module, const fs::File& file) {
        std::string extension = file.extension();
        base::StrID ext_id(extension.c_str());

        CORE_ASSERT(module->m_other_files.contains(ext_id), base::strConcat(
            "Other file with extension '", ext_id.strView(), "' does not exist in module ", module->getName().strView()
        ));
        
            auto& files = module->m_other_files.at(ext_id);
            auto it = std::ranges::find_if(files,
                                  [&file](const fs::File& f) {
                                      return f.getFilePath() == file.getFilePath();
                                  });

        CORE_ASSERT(it != files.end(), base::strConcat(
            "Other file with path '", file.getFilePath().string(), "' does not exist in module ", module->getName().strView()
        ));

        files.erase(it);
        //@TODO: do we need to update the module here?
        //module->update();
        return true;
    }

    bool ModuleTreeModifier::setParent(query::Context& ctx, base::Ref<ModuleTree> module, base::Optional<base::Ref<ModuleTree>> parent) {
        if(parent.has_value()) {
            addSubmodule(ctx, parent.value(), module);
        } else if(module->m_parent.has_value()) {
            removeParent(ctx, module);
        }
        return true;
    }

    bool ModuleTreeModifier::removeParent(query::Context& ctx, base::Ref<ModuleTree> module) {
        CORE_ASSERT(module->m_parent.has_value(), base::strConcat(
            "Module ", module->getName().strView(), " does not have a parent"
        ));

        //remove this module from its parent's submodules
        auto parent = module->m_parent.value();
        auto& submodules = parent->m_submodules;
        auto it = std::ranges::find_if(submodules,
                              [module](const auto& pair) {
                                  return pair.second->getID() == module->getID();
                              });
        CORE_ASSERT(it != submodules.end(), base::strConcat(
            "Submodule with ID ", std::to_string(module->getID().asInt()), " does not exist in parent module ", parent->getName().strView()
        ));
        submodules.erase(it);

        // Update parent module
        parent->update();
        parent->updateParentsModuleRecursively();

        module->m_parent = {};

        // Update the module itself
        module->update();
        module->updateAllSubmodulesRecursively();
        return true;
    }

    bool ModuleTreeModifier::removeModule(query::Context& ctx, ModuleID module_id) {
        if (!ModuleTree::module_map.contains(module_id)) {
            return false;
        }
        
        auto module_ref = ModuleTree::module_map.atMaybe(module_id).value();
        auto parent = module_ref->m_parent;
        
        // Remove from static map
        ModuleTree::module_map.erase(module_id);

        // Remove all source files associated with this module
        for (const auto& file : module_ref->m_source_files) {
            SourceFile::file_map.erase(file->getID());
        }

        // Remove main source file if it exists
        if(module_ref->m_main_source_file.has_value()) {
            SourceFile::file_map.erase(module_ref->m_main_source_file.value()->getID());
        }
        
        // Update parent module if it exists
        if (parent.has_value()) {
            // Remove the submodule from the parent's submodules
            auto& submodules = parent.value()->m_submodules;
            auto it = std::ranges::find_if(submodules,
                                  [module_id](const auto& pair) {
                                      return pair.second->getID() == module_id;
                                  });
            CORE_ASSERT(it != submodules.end(), base::strConcat(
                "Submodule with ID ", std::to_string(module_id.asInt()), " does not exist in parent module ", parent.value()->getName().strView()
            ));
            submodules.erase(it);
            parent.value()->update();
            parent.value()->updateParentsModuleRecursively();
        }
        
        return true;
    }

    void ModuleTreeModifier::fileModified(query::Context& ctx, const fs::File& file) {
        Ref<SourceFile> source_file = SourceFile::getSourceFile(file);
        CRef<ModuleTree> linked_module = source_file->getModule();
        Ref<ModuleTree> module = ModuleTree::module_map.atMaybe(linked_module->getID()).value();
        source_file->update(true);
        module->update();
        module->updateParentsModuleRecursively();
    }

    // ----------------------

    base::StrID moduleName(ModuleID module) {
        return ModuleTree::getModule(module)->getName();
    }

    std::string printModuleTree(ModuleID module) {
        return ModuleTree::getModule(module)->prettyPrint();
    }

    /*********************
    * QueryParentModule *
    *********************/
    struct IMPLEMENT_QUERY(QueryParentModule, base::Optional<ModuleID>) {
        static auto provide(Context&, QKey key) -> PResult {
            const auto& module_tree = ModuleTree::getModule(key);
            return module_tree->getParentModule().map([](const auto& parent) { return parent->getID(); }
            );
        }

        QUERY_AUTO_NO_CACHE
    };

    QUERY_IMPLEMENTATION_BOILERPLATE(QueryParentModule);

    /***********************
    * QueryMainSourceFile *
    ***********************/
    struct IMPLEMENT_QUERY(QueryMainSourceFile, FileID) {
        static auto provide(Context&, QKey key) -> PResult {
            const auto& module_tree = ModuleTree::getModule(key);
            return module_tree->getMainSourceFile()->getID();
        }

        QUERY_AUTO_NO_CACHE
    };

    QUERY_IMPLEMENTATION_BOILERPLATE(QueryMainSourceFile);

    /********************
    * QuerySourceFiles *
    ********************/
    struct IMPLEMENT_QUERY(QuerySourceFiles, std::vector<FileID>) {
        static auto provide(Context&, QKey key) -> PResult {
            const auto& module_tree = ModuleTree::getModule(key);

            std::vector<FileID> out{};
            for (const auto& file: module_tree->getSourceFiles()) out.push_back(file->getID());
            return out;
        }

        QUERY_AUTO_CACHE_REF
    };

    QUERY_IMPLEMENTATION_BOILERPLATE(QuerySourceFiles);

    /*******************
    * QuerySubmodules *
    *******************/
    struct IMPLEMENT_QUERY(QuerySubmodules, base::HashMap<base::StrID COMMA ModuleID>) {
        static auto provide(Context&, QKey key) -> PResult {
            const auto& module_tree = ModuleTree::getModule(key);

            PResult out{};
            for (const auto& [name, module]: module_tree->getSubmodules())
                out.put(name, module->getID());
            return out;
        }

        QUERY_AUTO_CACHE_REF
    };

    QUERY_IMPLEMENTATION_BOILERPLATE(QuerySubmodules);

    /****************
    * QueryFilePST *
    ****************/
    struct IMPLEMENT_QUERY(QueryFilePST, CRef<pst::PST<>>) {
        static auto provide(Context& ctx, QKey key) -> PResult {
            auto file = SourceFile::getSourceFile(key);
            auto pst  = file->getPST();
            root_element_file_back_map.put(pst->getRootElement().unlock(ctx)->getID(), key);

            // @todo modify it, when making proper helios errors
            if (pst->getLogger()->bad()) {
                std::cerr << "PARSING ERRORS: \n";
                pst->getLogger()->dumpLog(true, std::cerr);
                std::cerr << "\n\n";
            }

            return pst;
        }

        // @note: unstable ref here is only possible, because
        // PResult is already a reference
        QUERY_AUTO_CACHE_COPY
    };

    QUERY_IMPLEMENTATION_BOILERPLATE(QueryFilePST);

    ModuleID extendQueryModuleIDOfPST(
        query::Context& ctx, pst::AccessLocked<pst::LangElement> element
    ) {
        // get top-level:
        while (element.unlock(ctx)->getParent()) element = element.unlock(ctx)->getParent().value();

        // this access depends of global state that might become a problem in incremental compilation:
        auto file_id = root_element_file_back_map[element.unlock(ctx)->getID()];
        auto result  = SourceFile::getSourceFile(file_id)->getModule();
        CORE_ASSERT(result->getID().isGood(), "Bad module ID in SourceFile");

        return result->getID();
    }

    CRef<pst::PST<>> queryPSTFromFilePath(
        query::Context&, const fs::File& file_path
    ) {
        return SourceFile::getSourceFile(file_path)->getPST();
    }
}
