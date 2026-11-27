// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "module_tree.hpp"

#include <base/collections/optional.hpp>
#include <base/pointers/ref.hpp>

#include <filesystem/file.hpp>

namespace compiler::frontend {
	class SyntheticReplChainModuleTreeNode;

	/**
	 * @brief Modifications that work the same for every module kind, and the helpers shared by the
	 * per-kind modifiers (StandardModuleTreeModifier, ScriptModuleTreeModifier and
	 * SyntheticReplChainModuleTreeModifier).
	 *
	 * All modifications are performed in-place. This class cannot be instantiated.
	 * If you are using this class you should know what you are doing.
	 *
	 * @note The split of the modifiers per module kind is not a settled design. Nobody is attached
	 * to it, so reshape these classes freely when the code needs it.
	 */
	class ModuleTreeModifierCommon {
	public:
		/**
		 * @brief Detaches the given module from its parent, whatever the kind of the parent is.
		 * @param module The module to modify, it must have a parent.
		 */
		static void removeParent(base::Ref<ModuleTree> module);

		/**
		 * @brief Changes the package ID of the given module and ALL its child modules recursively.
		 * All modules in the same module tree must have the same package ID.
		 * @note This can only be done on root modules (modules without a parent).
		 * @param module The module to modify.
		 * @param new_package_id The new package ID to set.
		 */
		static void changePackageID(base::Ref<ModuleTree> module, base::StrID new_package_id);

		/**
		 * @brief Removes the given module and all of its child modules recursively.
		 * For a script or a REPL chain node, this removes the rest of its chain too.
		 * @param module The module to remove.
		 */
		static void removeModuleRecursive(base::Ref<ModuleTree> module);

		/**
		 * @brief Notifies that a file has been modified and updates its SourceFile.
		 * @param file The file that was modified.
		 */
		static void fileModified(const fs::File& file);

	protected:
		ModuleTreeModifierCommon() = default;

		/**
		 * @brief Panics if making `parent` the parent of `child` would create a cycle.
		 */
		static void assertNoCycle(base::Ref<ModuleTree> parent, base::Ref<ModuleTree> child);

		/**
		 * @brief Access the slot holding the next REPL chain node of a script or a chain node.
		 * @note Panics for standard modules.
		 */
		static Ref<base::Optional<base::Ref<SyntheticReplChainModuleTreeNode>>> replChildSlot(
			base::Ref<ModuleTree> module
		);

		/**
		 * @brief Removes `module` from the child list of its parent (submodules or REPL child),
		 * without touching `module` itself.
		 */
		static void unlinkFromParent(base::Ref<ModuleTree> module);

		/**
		 * @brief Removes the source files owned by `module` from storage.
		 * @note This invalidates the SourceFile instances!
		 */
		static void removeOwnedSourceFiles(base::Ref<ModuleTree> module);
	};

	/**
	 * @brief Modifier for standard modules (StandardModuleTreeNode).
	 *
	 * @note The API operates on the standard/script module layer and does not go into synthetic
	 * REPL chain modules unless explicitly stated.
	 */
	class StandardModuleTreeModifier final: public ModuleTreeModifierCommon {
	public:
		/**
		 * @brief Sets the main source file for the given module.
		 * @param module The module to modify.
		 * @param file The file to set as main source file.
		 */
		static void setMainSourceFile(base::Ref<ModuleTree> module, const fs::File& file);

		/**
		 * @brief Removes the main source file from the given module.
		 * @param module The module to modify.
		 */
		static void removeMainSourceFile(base::Ref<ModuleTree> module);

		/**
		 * @brief Adds a submodule (a standard or a script module) to the given module.
		 * @param module The module to modify.
		 * @param submodule The submodule to add, it must not have a parent yet.
		 */
		static void addSubmodule(base::Ref<ModuleTree> module, base::Ref<ModuleTree> submodule);

		/**
		 * @brief Adds an "other" file to the given module.
		 * @param module The module to modify.
		 * @param file The file to add.
		 */
		static void addOtherFile(base::Ref<ModuleTree> module, const fs::File& file);

		/**
		 * @brief Removes an "other" file from the given module.
		 * @param module The module to modify.
		 * @param file The file to remove.
		 */
		static void removeOtherFile(base::Ref<ModuleTree> module, const fs::File& file);

		/**
		 * @brief Sets the parent of the given module, adding it as a submodule of the parent.
		 * @param module The module to modify, it must not have a parent yet.
		 * @param parent The new parent module, must be a standard module.
		 */
		static void setParent(base::Ref<ModuleTree> module, base::Ref<ModuleTree> parent);

		/**
		 * @brief Removes the given module, and moves its submodules to its parent.
		 * Also removes it from its parent's submodules and deletes its source files.
		 * @param module The module to remove.
		 */
		static void removeSingleModule(base::Ref<ModuleTree> module);

	private:
		StandardModuleTreeModifier() = default;
	};

	/**
	 * @brief Modifier for script modules (ScriptModuleTreeNode).
	 */
	class ScriptModuleTreeModifier final: public ModuleTreeModifierCommon {
	public:
		/**
		 * @brief Sets the parent of the given script, adding it as a submodule of the parent.
		 * @param script The script module to modify, it must not have a parent yet.
		 * @param parent The new parent module, must be a standard module.
		 */
		static void setParent(base::Ref<ModuleTree> script, base::Ref<ModuleTree> parent);

	private:
		ScriptModuleTreeModifier() = default;
	};

	/**
	 * @brief Modifier for synthetic REPL chain modules (SyntheticReplChainModuleTreeNode).
	 */
	class SyntheticReplChainModuleTreeModifier final: public ModuleTreeModifierCommon {
	public:
		/**
		 * @brief Appends the given chain node to a chain.
		 * Sets the parent of `node` and makes `node` the REPL child of the parent.
		 * @param node The chain node to modify, it must not have a parent yet.
		 * @param parent The last node of the chain, or the script module that starts it. It must
		 * not have a REPL child yet.
		 */
		static void setParent(base::Ref<ModuleTree> node, base::Ref<ModuleTree> parent);

	private:
		SyntheticReplChainModuleTreeModifier() = default;
	};
}
