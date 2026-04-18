#include "module_tree.hpp"

#include "access.hpp"
#include "functors.hpp"
#include "module_flags/module_flags.hpp"
#include "queries.hpp"
#include "source_file.hpp"

#include <concurrent/base/collections/hash_map.hpp>
#include <concurrent/worker/worker_manager.hpp>
#include <frontend/pst_parser/pst_id.hpp>

#include <base/collections/stable_hashmap.hpp>
#include <base/config/build_type.hpp>
#include <base/except/exceptions.hpp>

#include <query_framework/standard_query/query_cache_macros.hpp>
#include <query_framework/standard_query/query_impl.hpp>
#include <string_id/string_id.hpp>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <ranges>
#include <regex>
#include <sstream>

namespace {
	/**
	 * @brief Map storing FileID of each parsed PST (by root element ID)
	 * @note: as of right now it is needed only for QueryPrimaryCodeScopeFor for acquiring
	 * the root scope via extendQueryModuleIDOfPST.
	 * @todo: Either delete root scopes and add to PST some kind of "module nodes" or put
	 * information from this map into PST nodes.
	 *
	 * \parallel A map from PST root element IDs back to FileIDs, stored at module-tree level. Used
	 * during PST construction/association; must be safe if PST is built concurrently.
	 */
	inline static concurrent::ConHashMap<pst::PstID, compiler::frontend::FileID>
		root_element_file_back_map;

	/**
	 * StableHashMap that stores all ModuleTree instances.
	 */
	base::StableHashMap<usize, compiler::frontend::ModuleTree> modules;
	usize                                                      next_module_storage_key = 0;

	/**
	 * Checks if a file name is valid according to the reject regex.
	 * @param filename The file name to check.
	 * @param reject_file_regex The regex to use for rejection.
	 * @return True if valid, false otherwise.
	 */
	bool isFileNameValid(const std::string& filename, const std::regex& reject_file_regex) {
		std::smatch match;
		return !std::regex_match(filename, match, reject_file_regex);
	}

	/**
	 * Checks if a directory name is valid according to the reject regex.
	 * @param dirname The directory name to check.
	 * @param reject_directory_regex The regex to use for rejection.
	 * @return True if valid, false otherwise.
	 */
	bool isDirectoryNameValid(const std::string& dirname, const std::regex& reject_directory_regex) {
		std::smatch match;
		return !std::regex_match(dirname, match, reject_directory_regex);
	}
}

namespace compiler::frontend {

	const hashing::ComponentHash& ModuleTree::getPathComponentHash(ModuleID module_id) {
		Ref<ModuleTree> module = module_id.ref;
		module->updateModuleHashFromRootToThis();
		CORE_ASSERT(
			module->m_path_component_hash.has_value(),
			"Component hash should have value after update!"
		);
		return module->m_path_component_hash.value();
	}

	const hashing::ComponentHash::HashType& ModuleTree::getModuleHash(ModuleID module_id) {
		Ref<ModuleTree> module = module_id.ref;
		module->updateModuleHashFromRootToThis();
		CORE_ASSERT(module->m_hash.has_value(), "Module hash should have value after update!");
		return module->m_hash.value();
	}

	Ref<ModuleTree> ModuleTreeBuilder::create(
		const fs::File&   root,
		std::string_view  package_id,
		const std::regex& file_reject,
		const std::regex& dir_reject
	) {
		base::Box<ModuleTreeBuilder> builder = ModuleTreeBuilder::create();

		if (root.isDirectory())
			builder->buildFromDirectory(root, package_id, file_reject, dir_reject);
		else
			builder->buildFromSingleFile(root, package_id);

		return builder->finalize();
	}

	Ref<ModuleTree> ModuleTreeBuilder::createWithRandomPackageID(
		const fs::File& root, const std::regex& file_reject, const std::regex& dir_reject
	) {
		return create(root, base::generateRandomString(32), file_reject, dir_reject);
	}

	ModuleTree::ModuleTree() = default;

	ModuleID ModuleTree::getModuleID() const { return m_id.value(); }

	base::Optional<ModuleAccessLocked> ModuleTree::getParentModule() const {
		if (m_parent.has_value()) return ModuleAccessLocked(m_parent.value()->getModuleID());
		return {};
	}

	bool ModuleTree::hasMainSourceFile() const { return m_main_source_file.has_value(); }

	FileAccessLocked ModuleTree::getMainSourceFile() const {
		CORE_ASSERT(m_main_source_file.has_value(), "Main source file does not exist!");
		return FileAccessLocked(m_main_source_file.value()->getFileID());
	}

	SourceFilesAccessLocked ModuleTree::getSourceFiles() const {
		std::vector<FileAccessLocked> files;
		files.reserve(m_source_files.size());
		for (const auto& file: m_source_files) files.emplace_back(file->getFileID());
		return { getModuleID(), std::move(files) };
	}

	SubmodulesAccessLocked ModuleTree::getSubmodules() const {
		std::vector<ModuleAccessLocked> submodules;
		submodules.reserve(m_submodules.size());
		for (const auto& [name, submodule]: m_submodules)
			submodules.emplace_back(submodule->getModuleID());
		return { getModuleID(), std::move(submodules) };
	}

	ModuleChildAccessLocked ModuleTree::getSubmoduleByName(base::StrID name) const {
		base::Optional<ModuleID> child;
		if (auto maybe = m_submodules.atMaybe(name); maybe.has_value())
			child = (*maybe.value())->getModuleID();
		return ModuleChildAccessLocked(getModuleID(), name, child);
	}

	const base::StableHashMap<base::StrID, std::vector<fs::File>>& ModuleTree::getOtherFiles() const {
		return m_other_files;
	}

	base::StrID ModuleTree::getName() const { return m_name; }

	std::string ModuleTree::prettyPrint(u32 indentation) const {
		std::stringstream output;

		std::string indent;
		for (u32 i = 0; i < indentation % 3; i++) indent += " ";
		for (u32 i = 0; i < indentation - (indentation % 3); i++)
			indent += (i % 3 == 0 ? "│" : " ");

		if (getName().isBad())
			output << indent << "/ [id: " << reinterpret_cast<u64>(this) << "]\n";
		else
			output << indent << getName().strView() << "/ [name: " << getName().strView() << "]\n";

		if (hasMainSourceFile())
			output << indent << "├> "
				   << getFileRef(getMainSourceFile().illegalAccess().getID())->file.name() << '\n';
		else
			output << indent << "├> Missing main module file!\n";

		for (const auto& file_ref: getSourceFiles().illegalAccess())
			output << indent << "├= " << getFileRef(file_ref.illegalAccess().getID())->file.name()
				   << '\n';

		for (const auto& [ext, files]: getOtherFiles())
			for (const auto& file: files) output << indent << "├─ " << file.name() << '\n';

		for (const auto& submodule_ref: getSubmodules().illegalAccess())
			output << getModuleRef(submodule_ref.illegalAccess().getID())
						  ->prettyPrint(indentation + 3);

		return output.str();
	}

	void ModuleTree::invalidateHash() {
		// If ModuleHash is invalid, then children are also invalid
		if (!m_path_component_hash.has_value()) {
			// m_hash should not have value if path component hash is invalid
			// The are calculated in the same function: updateModuleHash()
			CORE_ASSERT(!m_hash.has_value(), "Module hash have value!");

			// assert if children are invalid too

			for (auto& sf: m_source_files)
				CORE_ASSERT(!sf->component_hash.has_value(), "Child component hash have value!");
			if (m_main_source_file.has_value())
				CORE_ASSERT(
					!m_main_source_file.value()->component_hash.has_value(),
					"Child component hash have value!"
				);
			for (auto& [_, submodule]: m_submodules)
				CORE_ASSERT(
					!submodule->m_path_component_hash.has_value(), "Child component hash have value!"
				);
			return;
		}
		m_path_component_hash.reset();
		m_hash.reset();
		for (auto& sf: m_source_files) sf->invalidateComponentHash();
		if (m_main_source_file.has_value()) m_main_source_file.value()->invalidateComponentHash();
		for (auto& [_, submodule]: m_submodules) submodule->invalidateHash();
	}

	void ModuleTree::updateModuleHash() {
		// Component hash part
		// Get parent component hash if existsS
		if (m_parent.has_value()) {
			CORE_ASSERT(
				m_parent.value()->m_path_component_hash.has_value(),
				"Parent component hash should have value!"
			);
			m_path_component_hash.emplace(m_parent.value()->m_path_component_hash.value(), m_name);
		} else {
			// root module tree, use package id as base
			CORE_ASSERT(m_package_id.isGood(), "Package ID must be set for module tree!");
			m_path_component_hash.emplace(hashing::ComponentHash(m_package_id), m_name);
		}

		// Module hash part

		// @TODO: #1389 verify it

		// Copy the path component hash to the component hash
		hashing::ComponentHash::HashAlg partial = m_path_component_hash->partial;

		// If a Module has parent
		hashing::addToHash(partial, m_parent.has_value());

		// If a Module has a main source file
		hashing::addToHash(partial, hasMainSourceFile());

		// We do not need to add source files count or submodule count here,
		// Because there is a separate SideInput for that
		// And there is no other way to get those counts
		// Only by calling unlock on SourceFilesAccessLocked or SubmodulesAccessLocked

		// We do not need to add a module name and package_name, since they are already in the path
		// component hash

		m_hash = partial.finalize();
	}

	void ModuleTree::updateModuleHashFromRootToThis() {
		if (!m_path_component_hash.has_value()) {
			// iterate thru parents to find one with component hash set or reach root (go up)
			base::Ref<ModuleTree>              g_parent          = this;
			std::vector<base::Ref<ModuleTree>> modules_to_update = { g_parent };
			while (g_parent->m_parent.has_value()
			       && !g_parent->m_parent.value()->m_path_component_hash.has_value()) {
				g_parent = g_parent->m_parent.value();
				modules_to_update.push_back(g_parent);
			}
			// go from top module to bottom module, so its in linear time
			for (auto& it: std::ranges::reverse_view(modules_to_update)) it->updateModuleHash();
		}
	}

	void ModuleTree::removeModuleFromStorage(Ref<ModuleTree> module) {
		CORE_ASSERT(
			module->m_storage_handle.has_value(),
			"Attempted to remove ModuleTree without storage handle"
		);
		const auto storage_key = module->m_storage_handle.value();
		const bool erased      = modules.erase(storage_key);
		CORE_ASSERT(erased, "Failed to remove ModuleTree from storage");
	}

	void ModuleTree::checkDanglingReference(const base::Ref<ModuleTree>& candidate) {
		IF_BUILD_TYPE_DEV({
			// If we are not using module modifier, skip the check
			if (!use_module_modifier_remove) return;
			const auto* candidate_ptr = candidate.get();
			bool        is_tracked    = false;
			for (const auto& entry: modules) {
				if (&entry.value == candidate_ptr) {
					is_tracked = true;
					break;
				}
			}
			if (!is_tracked) CORE_PANIC("dangling reference used after removing ModuleTree");
		});
	}

	void ModuleTreeBuilder::buildFromDirectory(
		const fs::File&   directory,
		std::string_view  package_id,
		const std::regex& file_reject,
		const std::regex& dir_reject
	) {
		CORE_ASSERT(
			directory.isDirectory(),
			base::strConcat("Expected directory, got file: ", directory.getFilePath().string())
		);

		setName(base::StrID(directory.name().c_str()));
		setPackageID(package_id);

		// Process all files and directories in the current directory
		for (const auto& path: directory.listFilePaths()) {
			// Skip symlinks to avoid cycles
			if (path.isSymlink()) continue;

			fs::File file(path);

			if (file.isDirectory()) {
				// Handle subdirectory
				if (!isDirectoryNameValid(file.name(), dir_reject)) continue;

				// build sub-module from directory
				base::Box<ModuleTreeBuilder> submodule_builder = ModuleTreeBuilder::create();
				submodule_builder->buildFromDirectory(file, package_id, file_reject, dir_reject);
				auto submodule = submodule_builder->finalize();
				CORE_ASSERT(
					submodule->getName() == base::StrID(file.name().c_str()),
					"Submodule name does not match"
				);

				// Discards directories without main module file:
				// @TODO: decide if this behavior is desirable
				if (submodule->hasMainSourceFile()) addSubmodule(submodule);
			} else {
				// Handle regular file
				if (!isFileNameValid(file.name(), file_reject)) continue;
				handleNewFile(file);
			}
		}
	}

	void ModuleTreeBuilder::buildFromSingleFile(const fs::File& file, std::string_view package_id) {
		std::string stem      = file.stem();
		std::string extension = file.extension();
		CORE_ASSERT(
			extension == LANG_MODULE_FILE,
			"Expected a module file, got: " + file.getFilePath().string()
		);
		setName(base::StrID(stem.c_str()));
		setPackageID(package_id);
		setMainSourceFile(file);
	}

	void ModuleTreeBuilder::handleNewFile(const fs::File& file) {
		CORE_ASSERT(
			file.isFile(),
			base::strConcat("Expected file, got directory: ", file.getFilePath().string())
		);
		std::string stem      = file.stem();
		std::string extension = file.extension();

		if (extension == LANG_SOURCE_FILE) {
			// Regular source file - store path for later
			addSourceFile(file);
		} else if (extension == LANG_MODULE_FILE) {
			// Module file
			base::StrID stem_id(stem.c_str());

			if (stem_id == m_name) {
				setMainSourceFile(file);
			} else {
				base::Box<ModuleTreeBuilder> submodule_builder = ModuleTreeBuilder::create();
				submodule_builder->buildFromSingleFile(file, this->m_package_id.strView());
				auto submodule = submodule_builder->finalize();
				CORE_ASSERT(submodule->getName() == stem_id, "Submodule name does not match");
				addSubmodule(base::Ref<ModuleTree>(submodule));
			}
		} else {
			// Other file
			addOtherFile(file);
		}
	}

	/*********************
	 * ModuleTreeBuilder Implementation
	 *********************/

	ModuleTreeBuilder::ModuleTreeBuilder(): m_finalized(false) {}

	base::Box<ModuleTreeBuilder> ModuleTreeBuilder::create() {
		return base::makeBox<ModuleTreeBuilder>(ModuleTreeBuilder());
	}

	base::Box<ModuleTreeBuilder> ModuleTreeBuilder::createWithRandomPackageID() {
		base::Box<ModuleTreeBuilder> builder = ModuleTreeBuilder::create();
		builder->setPackageID(base::generateRandomString(32));
		return builder;
	}

	void ModuleTreeBuilder::addSourceFile(const fs::File& file) {
		CORE_ASSERT(!m_finalized, "Builder already finalized");
		m_source_file_paths.push_back(file);
	}

	void ModuleTreeBuilder::setMainSourceFile(const fs::File& file) {
		CORE_ASSERT(!m_finalized, "Builder already finalized");
		CORE_ASSERT(!m_main_source_file_path.has_value(), "Main source file already set");
		m_main_source_file_path = file;
	}

	void ModuleTreeBuilder::addSubmodule(base::Ref<ModuleTree> submodule) {
		CORE_ASSERT(!m_finalized, "Builder already finalized");
		CORE_ASSERT(
			!m_submodules.contains(submodule->getName()),
			"Submodule with the same name already added"
		);
		m_submodules.put(submodule->getName(), submodule);
	}

	void ModuleTreeBuilder::addOtherFile(const fs::File& file) {
		CORE_ASSERT(!m_finalized, "Builder already finalized");
		std::string extension = file.extension();
		base::StrID ext_id(extension.c_str());

		if (!m_other_files.contains(ext_id)) m_other_files.put(ext_id, std::vector<fs::File>());
		m_other_files.at(ext_id).push_back(file);
	}

	void ModuleTreeBuilder::setName(base::StrID name) {
		CORE_ASSERT(!m_finalized, "Builder already finalized");
		CORE_ASSERT(m_name.isBad(), "Module name is already set");
		m_name = name;
	}

	void ModuleTreeBuilder::setParent(base::Ref<ModuleTree> parent) {
		CORE_ASSERT(!m_finalized, "Builder already finalized");
		m_parent = parent;
	}

	void ModuleTreeBuilder::setReplModule(const ReplData& repl_data) {
		CORE_ASSERT(!m_finalized, "Builder already finalized");
		m_repl_data = repl_data;
	}

	void ModuleTreeBuilder::setPackageID(std::string_view package_id) {
		CORE_ASSERT(!m_finalized, "Builder already finalized");
		CORE_ASSERT(m_package_id.isBad(), "Package ID is already set");
		m_package_id = base::StrID(std::string(package_id).c_str());
	}

	bool ModuleTreeBuilder::isFinalized() const { return m_finalized; }

	base::Ref<ModuleTree> ModuleTreeBuilder::finalize() {
		CORE_ASSERT(!m_finalized, "Builder already finalized");

		m_finalized = true;

		// Create new ModuleTree instance
		const auto      storage_key  = next_module_storage_key++;
		auto            inserted     = modules.put(storage_key, ModuleTree());
		Ref<ModuleTree> module_ref   = &inserted->value;
		module_ref->m_storage_handle = storage_key;
		ModuleID mod_id(module_ref);

		module_ref->m_id = mod_id;

		// Set ID and name
		module_ref->m_name        = m_name;
		module_ref->m_other_files = std::move(m_other_files);

		CORE_ASSERT(m_package_id.isGood(), "Package ID must be set for every module tree!");
		module_ref->m_package_id = m_package_id;

		// Set REPL-specific attributes
		module_ref->m_repl_data = m_repl_data;

		if (m_parent.has_value()) ModuleTreeModifier::setParent(module_ref, m_parent);

		// Create SourceFiles from stored paths
		if (m_main_source_file_path.has_value()) {
			module_ref->m_main_source_file
				= SourceFile::create(m_main_source_file_path.value(), mod_id);
		}

		for (const auto& file_path: m_source_file_paths) {
			auto source_file = SourceFile::create(file_path, mod_id);
			module_ref->m_source_files.push_back(source_file);
		}

		for (const auto& [name, submodule]: m_submodules)
			ModuleTreeModifier::addSubmodule(module_ref, submodule);

		return module_ref;
	}

	/*********************
	 * ModuleTreeModifier Implementation
	 *********************/

	void ModuleTreeModifier::addSourceFile(base::Ref<ModuleTree> module, const fs::File& file) {
		module->m_source_files.push_back(SourceFile::create(file, ModuleID(module)));
		module->updateModuleHash();
	}

	void ModuleTreeModifier::removeSourceFileFromStorage(base::Ref<SourceFile> file) {
		CORE_ASSERT(
			use_module_modifier_remove, "Module modifier feature is disabled. See module_flags.hpp"
		);

		Ref<ModuleTree> module
			= GetModuleID_Functor::getModRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
				file->getModule().illegalAccess().getID()
			);

		auto& source_files = module->m_source_files;
		auto  it
			= std::ranges::find_if(source_files, [file](const base::Ref<SourceFile>& source_file) {
				  return source_file == file;
			  });

		CORE_ASSERT(it != source_files.end(), "SourceFile not found in module");

		// Remove file from source_files
		source_files.erase(it);

		// Update module hash
		module->updateModuleHash();

		// Remove entry from root_element_file_back_map if exists
		if (auto root_id = file->getPST()->getRootElement().illegalAccess(); root_id.has_value())
			root_element_file_back_map.erase(root_id.value()->getID());

		// Remove SourceFile from storage. This invalidates the SourceFile instance!
		SourceFile::removeSourceFileFromStorage(file);
	}

	void ModuleTreeModifier::setMainSourceFile(base::Ref<ModuleTree> module, const fs::File& file) {
		CORE_ASSERT(
			!module->m_main_source_file.has_value(),
			"Main source file is already set, remove it first"
		);
		module->m_main_source_file = SourceFile::create(file, ModuleID(module));
		module->updateModuleHash();
	}

	void ModuleTreeModifier::addSubmodule(
		base::Ref<ModuleTree> module, base::Ref<ModuleTree> submodule
	) {
		base::StrID name = submodule->getName();
		CORE_ASSERT(
			!module->m_submodules.contains(name),
			base::strConcat(
				"Submodule with name '",
				name.strView(),
				"' already exists in module ",
				module->getName().strView(),
				" call remove first!"
			)
		);


		module->m_submodules.put(name, submodule);

		CORE_ASSERT(
			!submodule->m_parent.has_value(),
			base::strConcat("Submodule ", name.strView(), " already has a parent")
		);
		submodule->m_parent = module;

		CORE_ASSERT(
			submodule->m_package_id == module->m_package_id,
			"Submodule package ID must match parent module package ID"
		);

		// we need to detect cycles as someone could accidentally create one
		// for example if module A is parent of B in original module tree
		// and function addSubmodule(B, A) is called
		// we would have a cycle A -> B -> A
		// this is a programer error, because cycle is not possible in standard module tree from
		// path creation
		auto current = module;
		while (current->m_parent.has_value()) {
			if (current->m_parent.value() == submodule) {
				CORE_PANIC(
					"Adding submodule '",
					submodule->getName().strView(),
					"' to module '",
					module->getName().strView(),
					"' would create a cycle in the module tree!"
				);
			}
			current = current->m_parent.value();
		}

		// Invalidate component hash for the submodule and its children
		submodule->invalidateHash();

		// Update module hash for the parent module since the number of children changed
		// Adding a submodule does not change the path component hash of the module so we do not
		// need to invalidate hash For all SourceFiles and Submodules
		// @TODO: #1253 every update and change to module should invalidate query caches
		module->updateModuleHash();
	}

	void ModuleTreeModifier::addOtherFile(base::Ref<ModuleTree> module, const fs::File& file) {
		std::string extension = file.extension();
		base::StrID ext_id(extension.c_str());

		if (!module->m_other_files.contains(ext_id))
			module->m_other_files.put(ext_id, std::vector<fs::File>());

		CORE_ASSERT(
			!std::ranges::any_of(
				module->m_other_files.at(ext_id),
				[&file](const fs::File& f) { return f.getFilePath() == file.getFilePath(); }
			),
			base::strConcat(
				"Other file with path '",
				file.getFilePath().string(),
				"' already exists in module ",
				module->getName().strView()
			)
		);

		module->m_other_files.at(ext_id).push_back(file);
		//@TODO: do we need to update the module here? #1253
		// module->update();
	}

	void ModuleTreeModifier::removeMainSourceFile(base::Ref<ModuleTree> module) {
		CORE_ASSERT(
			use_module_modifier_remove, "Module modifier feature is disabled. See module_flags.hpp"
		);
		CORE_ASSERT(
			module->m_main_source_file.has_value(),
			base::strConcat(
				"Module ", module->getName().strView(), " does not have a main source file"
			)
		);
		// Remove SourceFile from storage. This invalidates the SourceFile instance!
		SourceFile::removeSourceFileFromStorage(module->m_main_source_file.value());
		module->m_main_source_file = {};
		module->updateModuleHash();
	}

	void ModuleTreeModifier::removeOtherFile(base::Ref<ModuleTree> module, const fs::File& file) {
		std::string extension = file.extension();
		base::StrID ext_id(extension.c_str());

		CORE_ASSERT(
			module->m_other_files.contains(ext_id),
			base::strConcat(
				"Other file with extension '",
				ext_id.strView(),
				"' does not exist in module ",
				module->getName().strView()
			)
		);

		auto& files = module->m_other_files.at(ext_id);
		auto  it    = std::ranges::find_if(files, [&file](const fs::File& f) {
            return f.getFilePath() == file.getFilePath();
        });

		CORE_ASSERT(
			it != files.end(),
			base::strConcat(
				"Other file with path '",
				file.getFilePath().string(),
				"' does not exist in module ",
				module->getName().strView()
			)
		);

		files.erase(it);
		//@TODO: do we need to update the module here? #1253
		// module->update();
	}

	void ModuleTreeModifier::setParent(
		base::Ref<ModuleTree> module, base::Optional<base::Ref<ModuleTree>> parent
	) {
		CORE_ASSERT(parent.has_value(), "Parent module must be specified");
		addSubmodule(parent.value(), module);
	}

	void ModuleTreeModifier::removeParent(base::Ref<ModuleTree> module) {
		CORE_ASSERT(
			module->m_parent.has_value(),
			base::strConcat("Module ", module->getName().strView(), " does not have a parent")
		);

		// remove this module from its parent's submodules
		auto  parent     = module->m_parent.value();
		auto& submodules = parent->m_submodules;
		auto  it = submodules.findIf([&module](const auto& kv) { return kv.value == module; });

		CORE_ASSERT(
			it != submodules.end(),
			base::strConcat(
				"Submodule with name ",
				module->getName(),
				" does not exist in parent module ",
				parent->getName().strView()
			)
		);
		submodules.erase(it->key);

		module->m_parent = {};

		// Invalidate component hash for the module and its children as the parent changed
		module->invalidateHash();

		// Update module hash for the parent module since the number of children changed
		parent->updateModuleHash();
	}

	void ModuleTreeModifier::changePackageID(
		base::Ref<ModuleTree> module, std::string_view new_package_id
	) {
		CORE_ASSERT(
			!module->m_parent.has_value(),
			"Only root modules can have their package ID changed, the parent is: ",
			module->m_parent.value()->getName().strView()
		);

		std::function<void(base::Ref<ModuleTree>, std::string_view)> change_package_id =
			[&](base::Ref<ModuleTree> internal, std::string_view internal_new_package_id) {
				CORE_ASSERT(
					internal->m_package_id.isGood(), "Module does not have a valid package ID"
				);
				internal->m_package_id = base::StrID(std::string(internal_new_package_id).c_str());

				// Change the package ID for all submodules recursively
				for (auto& [_, submodule]: internal->m_submodules)
					change_package_id(submodule, internal_new_package_id);

				// Invalidate component hash for the module and its children as the package ID changed
				internal->invalidateHash();
			};

		change_package_id(module, new_package_id);
	}

	void ModuleTreeModifier::removeSingleModule(base::Ref<ModuleTree> module) {
		CORE_ASSERT(
			use_module_modifier_remove, "Module modifier feature is disabled. See module_flags.hpp"
		);
		auto parent = module->m_parent;

		// Update parent module if it exists
		if (parent.has_value()) {
			// Remove the submodule from the parent's submodules
			auto& submodules = parent.value()->m_submodules;
			auto  it = submodules.findIf([&module](const auto& kv) { return kv.value == module; });

			CORE_ASSERT(
				it != submodules.end(),
				base::strConcat(
					"Submodule with Name ",
					module->getName(),
					" and hash ",
					ModuleID(module).queryUnstablePerfectHash(),
					" does not exist in parent module ",
					parent.value()->getName().strView()
				)
			);
			submodules.erase(it->key);
		}

		// Change the parent of all submodules to the parent of the removed module
		for (auto& [_, submodule]: module->m_submodules) {
			if (parent.has_value()) {
				parent.value()->m_submodules.put(submodule->getName(), submodule);
				submodule->m_parent = parent.value();
			} else {
				submodule->m_parent = {};
			}
			submodule->invalidateHash();  // invalidate hash as parent changed
		}

		if (parent.has_value()) {
			// Update module hash for the parent module since the number of children changed
			parent.value()->updateModuleHash();
		}

		// Remove all source files from storage this will invalidate the SourceFile instances!
		for (auto& source_file: module->m_source_files)
			SourceFile::removeSourceFileFromStorage(source_file);

		// Remove main source file. This will invalidate the SourceFile instance!
		if (module->m_main_source_file.has_value())
			SourceFile::removeSourceFileFromStorage(module->m_main_source_file.value());

		// Remove the module from storage. This will invalidate the ModuleTree instance!
		ModuleTree::removeModuleFromStorage(module);
	}

	void ModuleTreeModifier::removeModuleRecursive(base::Ref<ModuleTree> module) {
		CORE_ASSERT(
			use_module_modifier_remove, "Module modifier feature is disabled. See module_flags.hpp"
		);
		auto parent = module->m_parent;

		if (parent.has_value()) {
			auto& submodules = parent.value()->m_submodules;
			auto  it = submodules.findIf([&module](const auto& kv) { return kv.value == module; });

			CORE_ASSERT(
				it != submodules.end(),
				base::strConcat(
					"Submodule with Name ",
					module->getName(),
					" and hash ",
					ModuleID(module).queryUnstablePerfectHash(),
					" does not exist in parent module ",
					parent.value()->getName().strView()
				)
			);
			submodules.erase(it->key);
			parent.value()->updateModuleHash();
		}

		auto recursive_delete = [&](auto&& self, base::Ref<ModuleTree> current) -> void {
			for (auto& [_, child]: current->m_submodules) self(self, child);

			for (auto& source_file: current->m_source_files)
				SourceFile::removeSourceFileFromStorage(source_file);
			if (current->m_main_source_file.has_value())
				SourceFile::removeSourceFileFromStorage(current->m_main_source_file.value());

			ModuleTree::removeModuleFromStorage(current);
		};

		recursive_delete(recursive_delete, module);
	}

	void ModuleTreeModifier::fileModified(const fs::File& file) {
		std::vector<Ref<SourceFile>> source_files = SourceFile::getSourceFilesFromFile(file);
		for (auto& source_file: source_files) source_file->update();
	}

	// ----------------------

	base::StrID moduleName(ModuleID module) { return GetModuleID_Functor::get(module)->getName(); }

	std::string printModuleTree(ModuleID module) {
		return GetModuleID_Functor::get(module)->prettyPrint();
	}

	ModuleID createModuleTree(const fs::File& file, std::string_view package_id) {
		return ModuleTreeBuilder::create(file, package_id)->getModuleID();
	}

	void parseAllFilesInModuleTree(ModuleID module_id) {
		CORE_ASSERT(
			query::Context::getState().activeQueryCount() == 0,
			"parseAllFilesInModuleTree called from within a query!"
		);

		// First collect all files to be parsed.
		std::vector<FileID> files_to_parse;
		auto                collect_files = [&](this auto&& self, ModuleID mid) -> void {
            auto module_tree = GetModuleID_Functor::get(mid);
            if (module_tree->hasMainSourceFile())
                files_to_parse.push_back(module_tree->getMainSourceFile().illegalAccess().getID());
            for (const auto& file: module_tree->getSourceFiles().illegalAccess())
                files_to_parse.push_back(file.illegalAccess().getID());
            for (const auto& submodule: module_tree->getSubmodules().illegalAccess())
                self(submodule.illegalAccess().getID());
		};
		collect_files(module_id);

		if (files_to_parse.empty()) return;

		// Now parse them concurrently.
		std::mutex              wait_mtx;
		std::condition_variable wait_cv;
		std::atomic<usize>      next_file_id{ 0 };
		bool                    all_files_parsed = false;
		auto&                   manager          = concurrent::worker::WorkerManager::get();

		auto schedule_next_file_parsing = [&](concurrent::worker::WRef worker) {
			usize idx = next_file_id.fetch_add(1, std::memory_order_relaxed);

			if (idx < files_to_parse.size()) {
				FileID file_id = files_to_parse[idx];

				worker->scheduleTask([&, file_id](concurrent::worker::WRef) {
					auto file_ref
						= GetFileID_Functor::getFileRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
							file_id
						);
					file_ref->getPST();
				});
			} else if (idx == files_to_parse.size() + concurrent::worker::getWorkerCount() - 1) {
				std::lock_guard<std::mutex> lock(wait_mtx);
				all_files_parsed = true;
				wait_cv.notify_one();
			}
		};

		manager.setNoTasksCallback(schedule_next_file_parsing);

		// Wait until all files are parsed.
		std::unique_lock lock(wait_mtx);
		wait_cv.wait(lock, [&] { return all_files_parsed; });

		// Clear the call back if all files are parsed.
		manager.setNoTasksCallback([](concurrent::worker::WRef) {});
	}

	ModuleID createModuleTreeWithRandomPackageID(const fs::File& file) {
		return ModuleTreeBuilder::createWithRandomPackageID(file)->getModuleID();
	}

	ModuleID createModuleTreeFromContents(
		std::string_view contents, base::Optional<std::string_view> package_id
	) {
		// createModuleTree function expects .dmf extension.
		auto virtual_file = fs::FileManager::createRandomVirtualFile(contents, ".dmf");

		if (!package_id.has_value())
			return createModuleTreeWithRandomPackageID(virtual_file);
		else
			return createModuleTree(virtual_file, package_id.value());
	}

	/*********************
	 * QueryParentModule *
	 *********************/
	struct IMPLEMENT_QUERY(QueryParentModule, base::Optional<ModuleID>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			auto module_tree = GetModuleID_Functor::get(key);
			auto parent      = module_tree->getParentModule();
			if (parent) return parent->unlock(ctx).getID();
			return {};
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryParentModule);

	/***********************
	 * QueryMainSourceFile *
	 ***********************/
	struct IMPLEMENT_QUERY(QueryMainSourceFile, FileID) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			auto module_tree = GetModuleID_Functor::get(key);
			auto file        = module_tree->getMainSourceFile();
			return file.unlock(ctx).getID();
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryMainSourceFile);

	/********************
	 * QuerySourceFiles *
	 ********************/
	struct IMPLEMENT_QUERY(QuerySourceFiles, std::vector<FileID>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			const auto&         module_tree = GetModuleID_Functor::get(key);
			std::vector<FileID> out;
			for (const auto& file: module_tree->getSourceFiles().unlock(ctx))
				out.emplace_back(file.unlock(ctx).getID());
			return out;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QuerySourceFiles);

	/*******************
	 * QuerySubmodules *
	 *******************/
	struct IMPLEMENT_QUERY(QuerySubmodules, base::StableHashMap<base::StrID COMMA ModuleID>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			const auto& module_tree = GetModuleID_Functor::get(key);

			PResult out{};
			for (const auto& module: module_tree->getSubmodules().unlock(ctx)) {
				auto module_id  = module.unlock(ctx).getID();
				auto module_ref = getModuleRef(module_id);
				out.put(module_ref->getName(), module_id);
			}
			return out;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QuerySubmodules);

	/****************
	 * getFilePST *
	 ****************/
	CRef<pst::PST<>> getFilePST(::query::Context& ctx, FileID file_id) {
		Ref<SourceFile> file
			= GetFileID_Functor::getFileRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
				file_id
			);
		auto pst           = file->getPST();
		auto root_optional = pst->getRootElement().unlockOpt(ctx);

		if (root_optional.has_value()) {
			auto root_id = root_optional.value()->getID();

			auto maybe_put_result = root_element_file_back_map.maybePut(root_id, file_id);
			if (!maybe_put_result) {
				// If the key already exists, assert that it maps to the same value
				CORE_ASSERT(
					root_element_file_back_map.getCopy(root_id) == file_id,
					"Root element ID already exists in back map with a different file ID"
				);
			}
		}

		return pst;
	}

	ModuleID extendQueryModuleIDOfPST(
		[[maybe_unused]] query::Context& ctx, pst::AccessLocked<pst::LangElement> element
	) {
		// get top-level:
		while (element.unlock(ctx)->getParent()) element = element.unlock(ctx)->getParent().value();

		// this access depends on the global state that might
		// become a problem in incremental compilation:
		auto file_id = root_element_file_back_map.getCopy(element.unlock(ctx)->getID());
		return getFileRef(file_id)->getModule().unlock(ctx).getID();
	}
}
