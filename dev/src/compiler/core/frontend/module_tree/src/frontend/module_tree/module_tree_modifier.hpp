#pragma once

#include "module_tree.hpp"

#include <base/collections/optional.hpp>
#include <base/pointers/ref.hpp>

#include <filesystem/file.hpp>

namespace compiler::frontend {
	/**
	 * @brief Modifier class for making changes to ModuleTree instances.
	 *
	 * @note The API is designed to operate on Script Module/Standard Module layer
	 * and don't go into synthetic REPL chain modules unless explicitly stated.
	 *
	 * ModuleTreeModifier provides static methods to add, remove, and update source files,
	 * submodules, parent relationships, and other files within a ModuleTree.
	 * All modifications are performed in-place and require a query context.
	 * This class cannot be instantiated.
	 * If you are using this class you should know what you are doing.
	 */
	class ModuleTreeModifier final {
	public:
		/**
		 * Sets the main source file for the given module.
		 * @param module The module to modify.
		 * @param file The file to set as main source file.
		 */
		static void setMainSourceFile(base::Ref<ModuleTree> module, const fs::File& file);

		/**
		 * Removes the main source file from the given module.
		 * @param module The module to modify.
		 */
		static void removeMainSourceFile(base::Ref<ModuleTree> module);

		/**
		 * Adds a submodule to the given module.
		 * @param module The module to modify.
		 * @param submodule The submodule to add.
		 */
		static void addSubmodule(base::Ref<ModuleTree> module, base::Ref<ModuleTree> submodule);

		/**
		 * Adds an "other" file to the given module.
		 * @param module The module to modify.
		 * @param file The file to add.
		 */
		static void addOtherFile(base::Ref<ModuleTree> module, const fs::File& file);

		/**
		 * Removes an "other" file from the given module.
		 * @param module The module to modify.
		 * @param file The file to remove.
		 */
		static void removeOtherFile(base::Ref<ModuleTree> module, const fs::File& file);

		/**
		 * Sets the parent of the given module.
		 * If parent is set, adds this module as a submodule to the parent.
		 * If parent is not set, removes the current parent.
		 * @param module The module to modify.
		 * @param parent The new parent module (optional).
		 */
		static void setParent(
			base::Ref<ModuleTree> module, base::Optional<base::Ref<ModuleTree>> parent
		);

		/**
		 * Removes the parent from the given module.
		 * @param module The module to modify.
		 */
		static void removeParent(base::Ref<ModuleTree> module);

		/**
		 * Changes the package ID of the given module and ALL its submodules recursively.
		 * All modules in the same module tree must have the same package ID.
		 * @note This can only be done on root modules (modules without a parent).
		 * @param module The module to modify.
		 * @param new_package_id The new package ID to set.
		 */
		static void changePackageID(base::Ref<ModuleTree> module, base::StrID new_package_id);

		/**
		 * Removes the module with the given ModuleID from the module map.
		 * Also removes it from its parent's submodules and deletes associated source files.
		 * @param module_id The ModuleID to remove.
		 * This will set the parent of all submodules to the parent of the removed module.
		 */
		static void removeSingleModule(base::Ref<ModuleTree> module);

		/**
		 * Removes the given module and all of its submodules recursively.
		 * Parent hashes are updated once after the entire subtree is removed.
		 * @param module_id The ModuleID to remove.
		 */
		static void removeModuleRecursive(base::Ref<ModuleTree> module);


		/**
		 * Notifies that a file has been modified and updates its SourceFile.
		 * @param file The file that was modified.
		 */
		static void fileModified(const fs::File& file);

	private:
		/**
		 * Private constructor to prevent instantiation.
		 */
		ModuleTreeModifier() = default;
	};
}
