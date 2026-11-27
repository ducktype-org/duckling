// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "module_id.hpp"
#include "module_tree.hpp"

#include <base/collections/hashmap.hpp>
#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>

#include <filesystem/file.hpp>

#include <regex>
#include <vector>

namespace compiler::frontend {
	class StandardModuleTreeNode;
	class ScriptModuleTreeNode;
	class SyntheticReplChainModuleTreeNode;

	/**
	 * @brief Common data and steps shared by the three module tree builders.
	 *
	 * Holds what every module kind has (name, package ID, parent) and the finalization steps that
	 * do not depend on the kind. It cannot be used on its own; use StandardModuleTreeBuilder,
	 * ScriptModuleTreeBuilder or SyntheticReplChainModuleTreeBuilder.
	 */
	class ModuleTreeBuilderCommon {
	public:
		/**
		 * @brief Sets the name of the module.
		 * @param name The name to set.
		 */
		void setName(base::StrID name);

		/**
		 * @brief Sets the package ID for the module tree.
		 * The package ID must be set for every module tree.
		 * @param package_id The package ID to set.
		 */
		void setPackageID(base::StrID package_id);

		/**
		 * @brief Checks if the builder is finalized.
		 * @return True if finalized, false otherwise.
		 */
		[[nodiscard]]
		bool isFinalized() const;

	protected:
		ModuleTreeBuilderCommon() = default;

		/**
		 * @brief Asserts that the builder can still be modified.
		 */
		void assertNotFinalized() const;

		/**
		 * @brief Stores the parent, which is linked to the module in finalize().
		 * @note The per-kind builders check the kind of the parent before calling this.
		 */
		void setParentCommon(base::Ref<ModuleTree> parent);

		/**
		 * @brief Marks the builder as finalized, stores the node and fills in the common data.
		 * @param node Freshly created node of the builder's kind.
		 * @return Reference to the stored node. The parent is NOT linked yet, the caller does it
		 * with the matching modifier once the kind-specific data is in place.
		 */
		base::Ref<ModuleTree> finalizeCommon(base::Box<ModuleTree> node);

		base::Optional<base::Ref<ModuleTree>> m_parent;
		base::StrID                           m_package_id;
		base::StrID                           m_name;
		bool                                  m_finalized = false;
	};

	/**
	 * @brief Builder for standard modules (StandardModuleTreeNode).
	 *
	 * Allows step-by-step construction of module trees with assertions that check the correctness
	 * of module creation. Also builds whole module trees from the file system.
	 */
	class StandardModuleTreeBuilder final: public ModuleTreeBuilderCommon {
	public:
		/**
		 * @brief Creates a new builder instance.
		 * @return Boxed StandardModuleTreeBuilder.
		 */
		static base::Box<StandardModuleTreeBuilder> create();

		/**
		 * @brief Creates a new builder instance with a random package ID.
		 * This is used for testing purposes.
		 * @return Boxed StandardModuleTreeBuilder.
		 */
		static base::Box<StandardModuleTreeBuilder> createWithRandomPackageID();

		/**
		 * @brief Factory method to create ModuleTree from filesystem tree.
		 * @param root Pre-constructed fs::File with a module structure.
		 * @param file_resolver Lambda, given a regular file on disk, may open a different file
		 * (useful in the LS). Never called for directories.
		 * @param file_reject Regex for rejecting files.
		 * @param dir_reject Regex for rejecting directories.
		 * @return A valid pointer with the root.
		 */
		static Ref<ModuleTree> create(
			const fs::File&     root,
			base::StrID         package_id,
			const FileResolver& file_resolver = identityFileResolver(),
			const std::regex&   file_reject   = DEFAULT_REJECT_FILE_REGEX,
			const std::regex&   dir_reject    = DEFAULT_REJECT_DIRECTORY_REGEX
		);

		/**
		 * @brief Factory method to create ModuleTree from filesystem tree with random package ID.
		 * This is used for testing purposes.
		 * @param root Pre-constructed fs::File with a module structure.
		 * @param file_reject Regex for rejecting files.
		 * @param dir_reject Regex for rejecting directories.
		 * @return A valid pointer with the root.
		 */
		static Ref<ModuleTree> createWithRandomPackageID(
			const fs::File&   root,
			const std::regex& file_reject = DEFAULT_REJECT_FILE_REGEX,
			const std::regex& dir_reject  = DEFAULT_REJECT_DIRECTORY_REGEX
		);

		/**
		 * @brief Sets the main source file for the module.
		 * @param file The main source file.
		 */
		void setMainSourceFile(const fs::File& file);

		/**
		 * @brief Adds a submodule (a standard or a script module) to the module being built.
		 * @param submodule The submodule to add.
		 */
		void addSubmodule(base::Ref<ModuleTree> submodule);

		/**
		 * @brief Adds an other file to the module being built.
		 * @param file The file to add.
		 */
		void addOtherFile(const fs::File& file);

		/**
		 * @brief Sets the parent module.
		 * @param parent The parent module, must be a standard module.
		 */
		void setParent(base::Ref<ModuleTree> parent);

		/**
		 * @brief Builds the module tree from a single file (single-file module).
		 * @param file The file to build from.
		 */
		void buildFromSingleFile(const fs::File& file, base::StrID package_id);

		/**
		 * @brief Finalizes the construction and returns the built module.
		 * After calling this, the builder becomes invalid.
		 * @return The constructed module.
		 */
		base::Ref<ModuleTree> finalize();

	private:
		StandardModuleTreeBuilder() = default;

		/**
		 * @brief Builds the module tree from a directory structure.
		 * This will recursively traverse the directory and build the module tree.
		 * @param directory The root directory to build the module tree from.
		 * @param file_resolver Lambda, given a regular file on disk, may open a different file
		 * instead.
		 * @param file_reject Regex for rejecting files.
		 * @param dir_reject Regex for rejecting directories.
		 */
		void buildFromDirectory(
			const fs::File&     directory,
			base::StrID         package_id,
			const FileResolver& file_resolver = identityFileResolver(),
			const std::regex&   file_reject   = DEFAULT_REJECT_FILE_REGEX,
			const std::regex&   dir_reject    = DEFAULT_REJECT_DIRECTORY_REGEX
		);

		/**
		 * @brief Handles a new file found during directory traversal.
		 * This is a helper function used when creating module tree from fs::File.
		 * @param file The file to handle.
		 */
		void handleNewFile(const fs::File& file);

		base::Optional<fs::File>                          m_main_source_file_path;
		base::HashMap<base::StrID, base::Ref<ModuleTree>> m_submodules;
		base::HashMap<base::StrID, std::vector<fs::File>> m_other_files;
	};

	/**
	 * @brief Builder for script modules (ScriptModuleTreeNode).
	 *
	 * @note The synthetic REPL chain of the script is not built here: chain nodes are built with
	 * SyntheticReplChainModuleTreeBuilder, with the script (or the previous chain node) as parent.
	 */
	class ScriptModuleTreeBuilder final: public ModuleTreeBuilderCommon {
	public:
		/**
		 * @brief Creates a new builder instance.
		 */
		static base::Box<ScriptModuleTreeBuilder> create();

		/**
		 * @brief Sets the parent module.
		 * @param parent The parent module, must be a standard module.
		 */
		void setParent(base::Ref<ModuleTree> parent);

		/**
		 * @brief Finalizes the construction and returns the built module.
		 * After calling this, the builder becomes invalid.
		 */
		base::Ref<ModuleTree> finalize();

	private:
		ScriptModuleTreeBuilder() = default;
	};

	/**
	 * @brief Builder for synthetic REPL chain modules (SyntheticReplChainModuleTreeNode).
	 *
	 * A chain node without a parent starts a new chain (the first statement of a REPL session).
	 */
	class SyntheticReplChainModuleTreeBuilder final: public ModuleTreeBuilderCommon {
	public:
		/**
		 * @brief Creates a new builder instance.
		 */
		static base::Box<SyntheticReplChainModuleTreeBuilder> create();

		/**
		 * @brief Sets the synthetic source file of this chain node.
		 */
		void setSyntheticSourceFile(const fs::File& file);

		/**
		 * @brief Sets the parent of this chain node.
		 * @param parent The previous node of the chain, or the script module that starts it.
		 */
		void setParent(base::Ref<ModuleTree> parent);

		/**
		 * @brief Same as setParent, but takes the ModuleID available in REPL code.
		 * @param parent The previous node of the chain, or the script module that starts it.
		 */
		void setParent(ModuleID parent);

		/**
		 * @brief Finalizes the construction and returns the built module.
		 * After calling this, the builder becomes invalid.
		 */
		base::Ref<ModuleTree> finalize();

	private:
		SyntheticReplChainModuleTreeBuilder() = default;

		base::Optional<fs::File> m_synthetic_source_file_path;
	};
}
