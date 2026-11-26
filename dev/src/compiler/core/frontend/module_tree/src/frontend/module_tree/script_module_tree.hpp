// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "module_tree.hpp"

namespace compiler::frontend {

	/**
	 * @brief REPL-specific module tree node.
	 *
	 * Represents a single node in the synthetic REPL chain.
	 * Only used for repl modules.
	 *
	 * @note Those nodes embed into module tree parent structure normally: m_parent is the previous
	 * node of the chain or the script module that starts the chain, and this node is the REPL child
	 * (m_repl_module_child) of its parent. The first statement of a REPL session has no parent.
	 */
	class SyntheticReplChainModuleTreeNode final: public ModuleTree {
		/**
		 * The synthetic source file this chain link was created from.
		 * It plays the role of the main source file of a standard module.
		 */
		base::Optional<base::Ref<SourceFile>> m_synthetic_source_file;

		/**
		 * The next node of the chain, if any.
		 */
		base::Optional<Ref<SyntheticReplChainModuleTreeNode>> m_repl_module_child;

		friend class ModuleTree;
		friend class SyntheticReplChainModuleTreeBuilder;
		friend class SyntheticReplChainModuleTreeModifier;
		friend class ModuleTreeModifierCommon;

		void makeAbstract() final;

	protected:
		std::vector<base::Ref<ModuleTree>> collectChildrenModules() const override;

		std::vector<base::Ref<SourceFile>> collectOwnedSourceFiles() const override;

		/**
		 * @brief Adds whether the parent is a REPL chain node, and if so, the parent's module
		 * hash.
		 */
		void addKindSpecificDataToHash(hashing::ComponentHash::HashAlg& partial) const override;

	public:
		explicit SyntheticReplChainModuleTreeNode();

		[[nodiscard]]
		const base::Optional<base::Ref<SourceFile>>& getSyntheticSourceFile() const;

		/**
		 * @brief The previous statement of the chain.
		 * @return The parent module if it is a REPL chain node, empty if the parent is the script
		 * that starts the chain or if there is no parent.
		 */
		[[nodiscard]]
		base::Optional<ModuleID> getReplModuleParent() const;
	};

	/**
	 * Script specific data.
	 *
	 * @note Scripts dont have a main source file like standard modules.
	 * Instead, a synthetic module chain is created to represent the script module.
	 * @note The parent of a script is a standard module (or none), like for standard modules.
	 */
	class ScriptModuleTreeNode final: public ModuleTree {
		base::Optional<fs::File> m_script_file;

		/**
		 * The first node of the synthetic REPL chain of this script, if any.
		 * It is set by SyntheticReplChainModuleTreeModifier::setParent.
		 */
		base::Optional<Ref<SyntheticReplChainModuleTreeNode>> m_repl_module_child;

		friend class ScriptModuleTreeBuilder;
		friend class ScriptModuleTreeModifier;
		friend class ModuleTreeModifierCommon;

		void makeAbstract() final;

	protected:
		[[nodiscard]]
		std::vector<base::Ref<ModuleTree>> collectChildrenModules() const override;

		/**
		 * @brief A script owns no source file itself, its synthetic chain modules own them.
		 */
		std::vector<base::Ref<SourceFile>> collectOwnedSourceFiles() const override;

		/**
		 * @brief Adds whether the script has a script file. The chain below the script is not
		 * hashed, it is made of child modules.
		 */
		void addKindSpecificDataToHash(hashing::ComponentHash::HashAlg& partial) const override;

	public:
		explicit ScriptModuleTreeNode();
	};


}
