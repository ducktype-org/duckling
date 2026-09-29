#pragma once

#include "module_tree.hpp"

namespace compiler::frontend {

	/**
	 * @brief REPL-specific module tree node.
	 * Represents a single node in the synthetic REPL chain.
	 * Only used for repl modules.
	 */
	class SyntheticReplChainModuleTreeNode: public ModuleTree {
		base::Optional<ModuleID> m_repl_module_parent;

	public:
		SyntheticReplChainModuleTreeNode(): ModuleTree() { kind = ModuleKind::ReplChain; }

		base::Optional<ModuleID> getReplModuleParent() const override {
			return m_repl_module_parent;
		}
	};

	/**
	 * Script specific data.
	 *
	 * @note Scripts dont have a main source file like standard modules.
	 * Instead, a synthetic module chain is created to represent the script module.
	 */
	class ScriptModuleTreeNode final: public ModuleTree {
		base::Optional<fs::File> m_script_file;

		/**
		 * @note: For REPL like execution this can be edited or extended via ModuleTreeModifier.
		 */
		std::vector<Box<SyntheticReplChainModuleTreeNode>> m_synthetic_repl_module_chain;

	public:
		ScriptModuleTreeNode(): ModuleTree() { kind = ModuleKind::Script; }
	};


}
