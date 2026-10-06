// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "module_tree_modifier.hpp"

#include "module_flags/module_flags.hpp"
#include "source_file.hpp"

#include <base/except/exceptions.hpp>

#include <algorithm>
#include <functional>
#include <ranges>
#include <string>
#include <vector>

namespace compiler::frontend {

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
					ModuleID(module).queryUnstablePerfectHash().toStringHex(),
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
					ModuleID(module).queryUnstablePerfectHash().toStringHex(),
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
}
