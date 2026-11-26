// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "module_tree_modifier.hpp"

#include "module_flags/module_flags.hpp"
#include "module_module_tree.hpp"
#include "module_tree.hpp"
#include "script_module_tree.hpp"
#include "source_file.hpp"

#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>

#include <string_id/string_id.hpp>

#include <algorithm>
#include <functional>
#include <ranges>
#include <string>
#include <vector>

namespace compiler::frontend {

	/*********************
	 * ModuleTreeModifierCommon
	 *********************/

	void ModuleTreeModifierCommon::assertNoCycle(
		base::Ref<ModuleTree> parent, base::Ref<ModuleTree> child
	) {
		// we need to detect cycles as someone could accidentally create one
		// for example if module A is parent of B in original module tree
		// and function addSubmodule(B, A) is called
		// we would have a cycle A -> B -> A
		// this is a programer error, because cycle is not possible in standard module tree from
		// path creation
		auto current = parent;
		while (true) {
			if (current == child) {
				CORE_PANIC(
					"Making '",
					parent->getName().strView(),
					"' the parent of '",
					child->getName().strView(),
					"' would create a cycle in the module tree!"
				);
			}
			if (!current->m_parent.has_value()) break;
			current = current->m_parent.value();
		}
	}

	Ref<base::Optional<base::Ref<SyntheticReplChainModuleTreeNode>>> ModuleTreeModifierCommon::replChildSlot(
		base::Ref<ModuleTree> module
	) {
		switch (module->getKind()) {
		case ModuleKind::Script:
			return &module->as<ScriptModuleTreeNode>()->m_repl_module_child;
		case ModuleKind::ReplChain:
			return &module->as<SyntheticReplChainModuleTreeNode>()->m_repl_module_child;
		default:
			CORE_PANIC("Only script and REPL chain modules have a REPL child");
		}
	}

	void ModuleTreeModifierCommon::unlinkFromParent(base::Ref<ModuleTree> module) {
		if (!module->m_parent.has_value()) return;
		auto parent = module->m_parent.value();

		if (parent->getKind() == ModuleKind::Module) {
			auto& submodules = parent->as<ModuleModuleTreeNode>()->m_submodules;
			auto  it         = std::ranges::find_if(submodules, [module](const auto& pair) {
                return pair.value == module;
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
			submodules.erase(base::StrID(it->key));
		} else {
			auto repl_child = replChildSlot(parent);
			CORE_ASSERT(
				repl_child->has_value() and repl_child->value() == module,
				base::strConcat(
					"Module ",
					module->getName(),
					" is not the REPL child of its parent ",
					parent->getName().strView()
				)
			);
			*repl_child = {};
		}
	}

	void ModuleTreeModifierCommon::removeOwnedSourceFiles(base::Ref<ModuleTree> module) {
		for (const auto& source_file: module->collectOwnedSourceFiles())
			SourceFile::removeSourceFileFromStorage(source_file);
	}

	void ModuleTreeModifierCommon::removeParent(base::Ref<ModuleTree> module) {
		CORE_ASSERT(
			module->m_parent.has_value(),
			base::strConcat("Module ", module->getName().strView(), " does not have a parent")
		);

		unlinkFromParent(module);
		module->m_parent = {};

		// Invalidate component hash for the module and its children as the parent changed
		module->invalidateHash();
	}

	void ModuleTreeModifierCommon::changePackageID(
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

				  // Change the package ID for all child modules recursively
				  for (auto& child: internal->collectChildrenModules())
					  change_package_id(child, internal_new_package_id);

				  // Invalidate component hash for the module and its children as the package ID changed
				  internal->invalidateHash();
			  };

		change_package_id(module, new_package_id);
	}

	void ModuleTreeModifierCommon::removeModuleRecursive(base::Ref<ModuleTree> module) {
		CORE_ASSERT(
			use_module_modifier_remove, "Module modifier feature is disabled. See module_flags.hpp"
		);

		unlinkFromParent(module);

		auto recursive_delete = [&](auto&& self, base::Ref<ModuleTree> current) -> void {
			for (auto& child: current->collectChildrenModules()) self(self, child);

			// Remove the source files of the module. This will invalidate the SourceFile instances!
			removeOwnedSourceFiles(current);

			// Remove the module from storage. This will invalidate the ModuleTree instance!
			ModuleTree::removeModuleFromStorage(current);
		};

		recursive_delete(recursive_delete, module);
	}

	void ModuleTreeModifierCommon::fileModified(const fs::File& file) {
		std::vector<Ref<SourceFile>> source_files
			= SourceFile::getSourceFilesFromPath(file.getFilePath());
		for (auto& source_file: source_files) source_file->update();
	}

	/*********************
	 * ModuleModuleTreeModifier
	 *********************/

	void ModuleModuleTreeModifier::setMainSourceFile(
		base::Ref<ModuleTree> module, const fs::File& file
	) {
		auto& main_source_file = module->as<ModuleModuleTreeNode>()->m_main_source_file;
		CORE_ASSERT(
			!main_source_file.has_value(), "Main source file is already set, remove it first"
		);
		main_source_file = SourceFile::create(file, module);
		module->invalidateHash();
		module->updateModuleHashFromRootToThis();
	}

	void ModuleModuleTreeModifier::removeMainSourceFile(base::Ref<ModuleTree> module) {
		CORE_ASSERT(
			use_module_modifier_remove, "Module modifier feature is disabled. See module_flags.hpp"
		);
		auto& main_source_file = module->as<ModuleModuleTreeNode>()->m_main_source_file;
		CORE_ASSERT(
			main_source_file.has_value(),
			base::strConcat(
				"Module ", module->getName().strView(), " does not have a main source file"
			)
		);
		// Remove SourceFile from storage. This invalidates the SourceFile instance!
		SourceFile::removeSourceFileFromStorage(main_source_file.value());
		main_source_file = {};
		module->invalidateHash();
		module->updateModuleHashFromRootToThis();
	}

	void ModuleModuleTreeModifier::addSubmodule(
		base::Ref<ModuleTree> module, base::Ref<ModuleTree> submodule
	) {
		CORE_ASSERT(
			submodule->getKind() == ModuleKind::Module or submodule->getKind() == ModuleKind::Script,
			"Only standard and script modules can be submodules"
		);

		base::StrID name       = submodule->getName();
		auto&       submodules = module->as<ModuleModuleTreeNode>()->m_submodules;
		CORE_ASSERT(
			!submodules.contains(name),
			base::strConcat(
				"Submodule with name '",
				name.strView(),
				"' already exists in module ",
				module->getName().strView(),
				" call remove first!"
			)
		);
		CORE_ASSERT(
			!submodule->m_parent.has_value(),
			base::strConcat("Submodule ", name.strView(), " already has a parent")
		);
		CORE_ASSERT(
			submodule->m_package_id == module->m_package_id,
			"Submodule package ID must match parent module package ID"
		);
		assertNoCycle(module, submodule);

		submodules.put(name, submodule);
		submodule->m_parent = module;

		// Invalidate component hash for the submodule and its children
		submodule->invalidateHash();
	}

	void ModuleModuleTreeModifier::addOtherFile(base::Ref<ModuleTree> module, const fs::File& file) {
		std::string extension = file.extension();
		base::StrID ext_id(extension.c_str());

		auto& other_files = module->as<ModuleModuleTreeNode>()->m_other_files;
		if (!other_files.contains(ext_id)) other_files.put(ext_id, std::vector<fs::File>());

		CORE_ASSERT(
			!std::ranges::any_of(
				other_files.at(ext_id),
				[&file](const fs::File& f) { return f.getFilePath() == file.getFilePath(); }
			),
			base::strConcat(
				"Other file with path '",
				file.getFilePath().string(),
				"' already exists in module ",
				module->getName().strView()
			)
		);

		other_files.at(ext_id).push_back(file);
	}

	void ModuleModuleTreeModifier::removeOtherFile(
		base::Ref<ModuleTree> module, const fs::File& file
	) {
		std::string extension = file.extension();
		base::StrID ext_id(extension.c_str());

		auto& other_files = module->as<ModuleModuleTreeNode>()->m_other_files;
		CORE_ASSERT(
			other_files.contains(ext_id),
			base::strConcat(
				"Other file with extension '",
				ext_id.strView(),
				"' does not exist in module ",
				module->getName().strView()
			)
		);

		auto& files = other_files.at(ext_id);
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

	void ModuleModuleTreeModifier::setParent(
		base::Ref<ModuleTree> module, base::Ref<ModuleTree> parent
	) {
		CORE_ASSERT(module->getKind() == ModuleKind::Module, "Module must be a standard module");
		addSubmodule(parent, module);
	}

	void ModuleModuleTreeModifier::removeSingleModule(base::Ref<ModuleTree> module) {
		CORE_ASSERT(
			use_module_modifier_remove, "Module modifier feature is disabled. See module_flags.hpp"
		);
		CORE_ASSERT(
			module->getKind() == ModuleKind::Module,
			"Only standard modules can be removed via this function, use removeModuleRecursive"
		);

		auto parent = module->m_parent;
		unlinkFromParent(module);

		// Change the parent of all child modules to the parent of the removed module
		for (auto& submodule: module->collectChildrenModules()) {
			submodule->m_parent = {};
			if (parent.has_value())
				addSubmodule(parent.value(), submodule);
			else
				submodule->invalidateHash();  // invalidate hash as parent changed
		}

		// Remove the source files of the module. This will invalidate the SourceFile instances!
		removeOwnedSourceFiles(module);

		// Remove the module from storage. This will invalidate the ModuleTree instance!
		ModuleTree::removeModuleFromStorage(module);
	}

	/*********************
	 * ScriptModuleTreeModifier
	 *********************/

	void ScriptModuleTreeModifier::setParent(
		base::Ref<ModuleTree> script, base::Ref<ModuleTree> parent
	) {
		CORE_ASSERT(script->getKind() == ModuleKind::Script, "Module must be a script module");
		ModuleModuleTreeModifier::addSubmodule(parent, script);
	}

	/*********************
	 * SyntheticReplChainModuleTreeModifier
	 *********************/

	void SyntheticReplChainModuleTreeModifier::setParent(
		base::Ref<ModuleTree> node, base::Ref<ModuleTree> parent
	) {
		CORE_ASSERT(node->getKind() == ModuleKind::ReplChain, "Module must be a REPL chain module");
		CORE_ASSERT(
			!node->m_parent.has_value(),
			base::strConcat("REPL chain module ", node->getName().strView(), " already has a parent")
		);
		CORE_ASSERT(
			node->m_package_id == parent->m_package_id,
			"REPL chain module package ID must match parent module package ID"
		);

		// Panics if the parent is neither a script nor a REPL chain module.
		auto repl_child = replChildSlot(parent);
		CORE_ASSERT(
			!repl_child->has_value(),
			base::strConcat(
				"Module ",
				parent->getName().strView(),
				" already has a REPL child, chains are linear"
			)
		);
		assertNoCycle(parent, node);

		*repl_child    = node->as<SyntheticReplChainModuleTreeNode>();
		node->m_parent = parent;

		// Invalidate component hash for the node and the rest of its chain
		node->invalidateHash();
	}
}
