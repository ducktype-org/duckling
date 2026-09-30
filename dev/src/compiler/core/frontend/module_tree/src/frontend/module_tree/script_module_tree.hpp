#pragma once

#include "module_tree.hpp"

namespace compiler::frontend {

	/**
	 * @brief REPL-specific module tree node.
	 * Represents a single node in the synthetic REPL chain.
	 * Only used for repl modules.
	 *
	 * @note: Parent of this module is either the REPL module higher in the chain or the script
	 * module itself.
	 */
	class SyntheticReplChainModuleTreeNode final: public ModuleTree {
		/**
		 * The synthetic source file this chain link was created from.
		 * It plays the role of the main source file of a standard module.
		 */
		base::Optional<base::Ref<SourceFile>> m_synthetic_source_file;

		base::Optional<Ref<SyntheticReplChainModuleTreeNode>> m_repl_module_child;

		void makeAbstract() final;

	protected:
		MRef<base::Optional<base::Ref<SourceFile>>> mainSourceFileSlot() override;

		MCRef<base::Optional<base::Ref<SourceFile>>> mainSourceFileSlot() const override;

		std::vector<base::Ref<ModuleTree>> collectChildrenModules() const override;


	public:
		explicit SyntheticReplChainModuleTreeNode();

		[[nodiscard]]
		const base::Optional<base::Ref<SourceFile>>& getSyntheticSourceFile() const;
	};

	/**
	 * Script specific data.
	 *
	 * @note Scripts dont have a main source file like standard modules.
	 * Instead, a synthetic module chain is created to represent the script module.
	 */
	class ScriptModuleTreeNode final: public ModuleTree {
		base::Optional<fs::File> m_script_file;

		base::Optional<Ref<SyntheticReplChainModuleTreeNode>> m_repl_module_child;

		void makeAbstract() final;


	public:
		ScriptModuleTreeNode();
	};


}
