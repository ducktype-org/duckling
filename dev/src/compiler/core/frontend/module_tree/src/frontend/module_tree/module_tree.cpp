#include "module_tree.hpp"

#include "access.hpp"
#include "functors.hpp"
#include "module_flags/module_flags.hpp"
#include "queries.hpp"
#include "source_file.hpp"

#include <concurrent/base/collections/hash_map.hpp>
#include <concurrent/worker/worker_manager.hpp>
#include <frontend/packages/access.hpp>

#include <base/collections/stable_hashmap.hpp>
#include <base/config/build_type.hpp>
#include <base/except/exceptions.hpp>

#include <query_framework/standard_query/query_cache_macros.hpp>
#include <query_framework/standard_query/query_impl.hpp>
#include <string_id/string_id.hpp>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <ranges>
#include <regex>
#include <sstream>
#include <string_view>

namespace {
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
	bool isFileNameValid(base::StrID filename, const std::regex& reject_file_regex) {
		auto view = filename.strView();
		return !std::regex_match(view.begin(), view.end(), reject_file_regex);
	}

	/**
	 * Checks if a directory name is valid according to the reject regex.
	 * @param dirname The directory name to check.
	 * @param reject_directory_regex The regex to use for rejection.
	 * @return True if valid, false otherwise.
	 */
	bool isDirectoryNameValid(base::StrID dirname, const std::regex& reject_directory_regex) {
		auto view = dirname.strView();
		return !std::regex_match(view.begin(), view.end(), reject_directory_regex);
	}
}

namespace compiler::frontend {

	const hashing::ComponentHash& ModuleTree::getPathComponentHash(ModuleID module_id) {
		Ref<ModuleTree> module = module_id.ref;
		// Update the component hash from root to this module if not valid
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

	const FileResolver& identityFileResolver() {
		static const FileResolver resolver
			= [](const fs::FilePath& disk_path) { return fs::File(disk_path); };
		return resolver;
	}

	Ref<ModuleTree> ModuleTreeBuilder::create(
		const fs::File&     root,
		base::StrID         package_id,
		const FileResolver& file_resolver,
		const std::regex&   file_reject,
		const std::regex&   dir_reject
	) {
		base::Box<ModuleTreeBuilder> builder = ModuleTreeBuilder::create();

		if (root.isDirectory())
			builder->buildFromDirectory(root, package_id, file_reject, dir_reject, file_resolver);
		else
			builder->buildFromSingleFile(root, package_id);

		return builder->finalize();
	}

	Ref<ModuleTree> ModuleTreeBuilder::createWithRandomPackageID(
		const fs::File& root, const std::regex& file_reject, const std::regex& dir_reject
	) {
		return create(root, base::StrID(base::generateRandomString(32)), identityFileResolver(), file_reject, dir_reject);
	}

	ModuleTree::ModuleTree(): m_hash_recompute_mutex(base::makeBox<std::mutex>()) {}

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

	SubmodulesAccessLocked ModuleTree::getSubmodules() const {
		std::vector<ModuleAccessLocked> submodules;
		submodules.reserve(m_submodules.size());
		for (const auto& [name, submodule]: m_submodules)
			submodules.emplace_back(submodule->getModuleID());
		return { getModuleID(), std::move(submodules) };
	}

	packages::PackageAccessLocked ModuleTree::getPackage() const {
		return packages::PackageAccessLocked(m_package_id);
	}

	ModuleChildAccessLocked ModuleTree::getSubmoduleByName(base::StrID name) const {
		base::Optional<ModuleID> child;
		if (auto maybe = m_submodules.atMaybe(name); maybe.has_value())
			child = (*maybe.value())->getModuleID();
		return ModuleChildAccessLocked(getModuleID(), name, child);
	}

	const base::HashMap<base::StrID, std::vector<fs::File>>& ModuleTree::getOtherFiles() const {
		return m_other_files;
	}

	base::StrID ModuleTree::getName() const { return m_name; }

	std::string ModuleTree::humanReadableID(query::Context& ctx) const {
		const auto own_name
			= getName().isBad() ? std::string_view("<unnamed>") : getName().strView();

		auto parent = getParentModule();
		if (parent.empty()) return base::strConcat(m_package_id.strView(), ".", own_name);

		const auto parent_id = parent.value().unlock(ctx).getID();
		return base::strConcat(getModuleRef(parent_id)->humanReadableID(ctx), ".", own_name);
	}

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
		if (m_main_source_file.has_value()) m_main_source_file.value()->invalidateComponentHash();
		for (auto& [_, submodule]: m_submodules) submodule->invalidateHash();
	}

	void ModuleTree::updateModuleHash() {
		// Component hash part
		// Get parent component hash if existsS
		if (m_parent.has_value()) {
			{
				IF_BUILD_TYPE_DEV(
					std::scoped_lock parent_lock(*m_parent.value()->m_hash_recompute_mutex);
					CORE_ASSERT(
						m_parent.value()->m_path_component_hash.has_value(),
						"Parent component hash should have value!"
					);
				);
			}
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

		// REPL metadata affects module semantics and therefore must affect module hash.
		hashing::addToHash(partial, m_repl_data.has_value());

		if (m_repl_data.has_value()) {
			hashing::addToHash(partial, m_repl_data->m_repl_module_parent.has_value());
			if (m_repl_data->m_repl_module_parent.has_value()) {
				auto repl_parent = m_repl_data->m_repl_module_parent.value();

				hashing::addToHash(partial, ModuleTree::getModuleHash(repl_parent));
			}
		}

		// We do not need to add source files count or submodule count here,
		// Because there is a separate SideInput for that
		// And there is no other way to get those counts
		// Only by calling unlock on SourceFilesAccessLocked or SubmodulesAccessLocked

		// We do not need to add a module name and package_name, since they are already in the path
		// component hash

		m_hash = partial.finalize();
	}

	void ModuleTree::updateModuleHashFromRootToThis() {
		std::scoped_lock lock(*m_hash_recompute_mutex);
		if (!m_path_component_hash.has_value()) {
			// iterate thru parents to find one with component hash set or reach root (go up)
			if (m_parent.has_value()) m_parent.value()->updateModuleHashFromRootToThis();
			// go from top module to bottom module, so its in linear time
			updateModuleHash();
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

	void ModuleTree::checkDanglingReference([[maybe_unused]] const base::Ref<ModuleTree>& candidate
	) {
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
		const fs::File&     directory,
		base::StrID         package_id,
		const std::regex&   file_reject,
		const std::regex&   dir_reject,
		const FileResolver& file_resolver
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

			fs::File file = file_resolver(path);

			if (file.isDirectory()) {
				// Handle subdirectory
				if (!isDirectoryNameValid(base::StrID(file.name()), dir_reject)) continue;

				// build sub-module from directory
				base::Box<ModuleTreeBuilder> submodule_builder = ModuleTreeBuilder::create();
				submodule_builder->buildFromDirectory(
					file, package_id, file_reject, dir_reject, file_resolver
				);
				auto submodule = submodule_builder->finalize();
				CORE_ASSERT(
					submodule->getName() == base::StrID(file.name()), "Submodule name does not match"
				);

				// Discards directories without main module file:
				// @TODO: decide if this behavior is desirable
				if (submodule->hasMainSourceFile()) addSubmodule(submodule);
			} else {
				// Handle regular file
				if (!isFileNameValid(base::StrID(file.name()), file_reject)) continue;
				handleNewFile(file);
			}
		}
	}

	void ModuleTreeBuilder::buildFromSingleFile(const fs::File& file, base::StrID package_id) {
		base::StrID stem      = base::StrID(file.stem());
		std::string extension = file.extension();
		CORE_ASSERT(
			extension == LANG_MODULE_FILE,
			"Expected a module file, got: " + file.getFilePath().string()
		);
		setName(stem);
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

		if (extension == LANG_MODULE_FILE) {
			// Module file
			auto stem_id = base::StrID(stem);

			if (stem_id == m_name) {
				setMainSourceFile(file);
			} else {
				base::Box<ModuleTreeBuilder> submodule_builder = ModuleTreeBuilder::create();
				submodule_builder->buildFromSingleFile(file, this->m_package_id);
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
		builder->setPackageID(base::StrID(base::generateRandomString(32)));
		return builder;
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

	void ModuleTreeBuilder::setPackageID(base::StrID package_id) {
		CORE_ASSERT(!m_finalized, "Builder already finalized");
		CORE_ASSERT(m_package_id.isBad(), "Package ID is already set");
		m_package_id = package_id;
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

		for (const auto& [name, submodule]: m_submodules)
			ModuleTreeModifier::addSubmodule(module_ref, submodule);

		return module_ref;
	}

	/*********************
	 * ModuleTreeModifier Implementation
	 *********************/


	void ModuleTreeModifier::setMainSourceFile(base::Ref<ModuleTree> module, const fs::File& file) {
		CORE_ASSERT(
			!module->m_main_source_file.has_value(),
			"Main source file is already set, remove it first"
		);
		module->m_main_source_file = SourceFile::create(file, ModuleID(module));
		module->updateModuleHashFromRootToThis();
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
		module->updateModuleHashFromRootToThis();
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
		auto  it         = std::ranges::find_if(submodules, [module](const auto& pair) {
            return pair.second == module;
        });
		CORE_ASSERT(
			it != submodules.end(),
			base::strConcat(
				"Submodule with name ",
				module->getName(),
				" does not exist in parent module ",
				parent->getName().strView()
			)
		);
		submodules.erase(it);

		module->m_parent = {};

		// Invalidate component hash for the module and its children as the parent changed
		module->invalidateHash();
	}

	void ModuleTreeModifier::changePackageID(
		base::Ref<ModuleTree> module, base::StrID new_package_id
	) {
		CORE_ASSERT(
			!module->m_parent.has_value(),
			"Only root modules can have their package ID changed, the parent is: ",
			module->m_parent.value()->getName().strView()
		);

		std::function<void(base::Ref<ModuleTree>, base::StrID)> change_package_id
			= [&](base::Ref<ModuleTree> internal, base::StrID internal_new_package_id) {
				  CORE_ASSERT(
					  internal->m_package_id.isGood(), "Module does not have a valid package ID"
				  );
				  internal->m_package_id = internal_new_package_id;

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
			auto  it         = std::ranges::find_if(submodules, [module](const auto& pair) {
                return pair.second == module;
            });
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
			submodules.erase(it);
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
			auto  it         = std::ranges::find_if(submodules, [module](const auto& pair) {
                return pair.second == module;
            });
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
			submodules.erase(it);
		}

		auto recursive_delete = [&](auto&& self, base::Ref<ModuleTree> current) -> void {
			for (auto& [_, child]: current->m_submodules) self(self, child);

			if (current->m_main_source_file.has_value())
				SourceFile::removeSourceFileFromStorage(current->m_main_source_file.value());

			ModuleTree::removeModuleFromStorage(current);
		};

		recursive_delete(recursive_delete, module);
	}

	void ModuleTreeModifier::fileModified(const fs::File& file) {
		std::vector<Ref<SourceFile>> source_files
			= SourceFile::getSourceFilesFromPath(file.getFilePath());
		for (auto& source_file: source_files) source_file->update();
	}

	// ----------------------

	base::StrID moduleName(ModuleID module) { return GetModuleID_Functor::get(module)->getName(); }

	std::string printModuleTree(ModuleID module) {
		return GetModuleID_Functor::get(module)->prettyPrint();
	}

	ModuleID createModuleTree(const fs::File& file, base::StrID package_id) {
		return ModuleTreeBuilder::create(file, package_id)->getModuleID();
	}

	void parseAllFilesInModuleTree(ModuleID module_id) {
		CORE_ASSERT(
			!query::Context::isAnyQueryCurrentlyRunning(),
			"parseAllFilesInModuleTree called when some query is currently running."
		);

		// First collect all files to be parsed.
		std::vector<FileID> files_to_parse;
		auto                collect_files = [&](this auto&& self, ModuleID mid) -> void {
            auto module_tree = GetModuleID_Functor::get(mid);
            if (module_tree->hasMainSourceFile())
                files_to_parse.push_back(module_tree->getMainSourceFile().illegalAccess().getID());
            for (const auto& submodule: module_tree->getSubmodules().illegalAccess())
                self(submodule.illegalAccess().getID());
		};
		collect_files(module_id);

		if (files_to_parse.empty()) return;

		// Now parse them concurrently.
		std::mutex              wait_mtx;
		std::condition_variable wait_cv;
		std::atomic<usize>      next_file_id{ 0 };
		std::atomic<usize>      parsed_files_count{ 0 };
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

					if (parsed_files_count.fetch_add(1, std::memory_order_relaxed) + 1
					    == files_to_parse.size()) {
						std::lock_guard<std::mutex> lock(wait_mtx);
						all_files_parsed = true;
						wait_cv.notify_one();
					}
				});
			}
		};

		manager.setNoTasksCallback(schedule_next_file_parsing);

		for (auto& worker: manager.getAllWorkers()) schedule_next_file_parsing(worker);

		// Wait until all files are parsed.
		{
			std::unique_lock lock(wait_mtx);
			wait_cv.wait(lock, [&] { return all_files_parsed; });
		}

		// Clear the callback so no worker schedules new parsing tasks.
		manager.setNoTasksCallback([](concurrent::worker::WRef) {});

		// All the lambdas above capture this function's stack frame by reference. A worker that
		// finished the last task may still be re-entering its loop and is about to run the
		// (now-cleared) no-tasks callback, or may have already copied the previous callback before
		// we cleared it (see Worker::run). Either way it would dereference references into this
		// frame. Wait until every worker is idle before returning, otherwise that frame gets
		// destroyed underneath them -> rare segfault.
		manager.waitForAllWorkersFree();
	}

	ModuleID createModuleTreeWithRandomPackageID(const fs::File& file) {
		return ModuleTreeBuilder::createWithRandomPackageID(file)->getModuleID();
	}

	ModuleID createModuleTreeFromContents(
		std::string_view contents, base::Optional<base::StrID> package_id
	) {
		// createModuleTree function expects .dk extension.
		auto virtual_file = fs::FileManager::createRandomVirtualFile(contents, LANG_MODULE_FILE);

		if (!package_id.has_value())
			return createModuleTreeWithRandomPackageID(virtual_file);
		else
			return createModuleTree(virtual_file, package_id.value());
	}

	/**********************
	 * QueryIsReplModule *
	 **********************/
	struct IMPLEMENT_QUERY(QueryIsReplModule, bool) {
		static auto provide(Context&, QKey key) -> PResult {
			// @TODO: #1389 verify Functor correctness.
			return GetModuleID_Functor::get(key)->isReplModule();
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryIsReplModule);

	/***************************
	 * QueryReplModuleParent *
	 ***************************/
	struct IMPLEMENT_QUERY(QueryReplModuleParent, base::Optional<ModuleID>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			// @TODO: #1389 verify Functor correctness.
			auto module_tree = GetModuleID_Functor::get(key);
			if (!module_tree->isReplModule()) return {};
			auto repl_parent = module_tree->getReplModuleParent();
			if (repl_parent.has_value()) {
				// Register dependency on the parent module contents so incremental rebuilds propagate
				ctx.query<QueryModuleSideInput>(KeyOf_ModuleSideInput{
					ModuleTree::getModuleHash(repl_parent.value()) });
			}
			return repl_parent;
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryReplModuleParent);

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

	/************************
	 * QueryPackageOfModule *
	 ************************/
	struct IMPLEMENT_QUERY(QueryPackageOfModule, packages::PackageAccessLocked) {
		static auto provide(Context&, QKey key) -> PResult {
			// @TODO: #3505 - currently changing package name/version doesn't invalidate stuff that
			// depends on those values
			return GetModuleID_Functor::get(key)->getPackage();
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryPackageOfModule);

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

	/*******************
	 * QuerySubmodules *
	 *******************/
	struct IMPLEMENT_QUERY(QuerySubmodules, base::HashMap<base::StrID COMMA ModuleID>) {
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
	CRef<pst::PST<>> getFilePST([[maybe_unused]] ::query::Context& ctx, FileID file_id) {
		Ref<SourceFile> file
			= GetFileID_Functor::getFileRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
				file_id
			);
		return file->getPST();
	}

}
