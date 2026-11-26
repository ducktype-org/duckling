// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "module_tree.hpp"

namespace compiler::frontend {

	/**
	 * @brief Standard module node.
	 */
	class ModuleModuleTreeNode final: public ModuleTree {
		/**
		 * The main source file of the module.
		 * @note This should always be set in standard compilation pipeline.
		 */
		base::Optional<base::Ref<SourceFile>> m_main_source_file;

		/**
		 * Submodules of the current module, indexed by their names
		 */
		base::HashMap<base::StrID, base::Ref<ModuleTree>> m_submodules;

		/**
		 * Other files in the module (not SourceFiles) currently nothing is
		 * happening with them. Do not use this in query unless AccessLocked is
		 * implemented for this
		 */
		base::HashMap<base::StrID, std::vector<fs::File>> m_other_files;

		void makeAbstract() final;

		friend class ModuleTree;
		friend class ModuleModuleTreeBuilder;
		friend class ModuleModuleTreeModifier;
		friend class ModuleTreeModifierCommon;

	protected:
		std::vector<base::Ref<ModuleTree>> collectChildrenModules() const override;

		std::vector<base::Ref<SourceFile>> collectOwnedSourceFiles() const override;

		/**
		 * @brief Adds nothing: everything a standard module hashes is in ModuleTree.
		 */
		void addKindSpecificDataToHash(hashing::ComponentHash::HashAlg& partial) const override;

	public:
		ModuleModuleTreeNode();

		const base::HashMap<base::StrID, std::vector<fs::File>>& getOtherFiles() const override;

		/**
		 * @brief Accesses the submodules located in this module.
		 *
		 * @note Use this only if you need all submodules. For single submodule access, use
		 * getSubmoduleByName().
		 * @return A lazy view that can be unlocked within a query context or accessed illegally
		 * (outside queries).
		 */
		[[nodiscard]]
		SubmodulesAccessLocked getSubmodules() const;

		/**
		 * @brief Access a single submodule edge by name.
		 *
		 * Registers dependency via QueryModuleChildSideInput when unlocked.
		 * Use this function in lookups when you need only a submodule with some name.
		 * @param name Name of the submodule to access.
		 * @return AccessLocked wrapper that may contain the submodule if it exists.
		 */
		[[nodiscard]]
		ModuleChildAccessLocked getSubmoduleByName(base::StrID name) const;
	};
}
