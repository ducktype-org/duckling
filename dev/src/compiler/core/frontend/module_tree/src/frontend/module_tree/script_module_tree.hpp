#pragma once

#include "module_tree.hpp"

namespace compiler::frontend {

	/**
	 * @brief REPL-specific module tree node.
	 * Represents a single node in the synthetic REPL chain.
	 * Only used for repl modules.
	 */
	class SyntheticReplChainModuleTreeNode: public ModuleTree {
		/**
		 * Parent REPL module in chronological order.
		 * Optional - only empty for first REPL module.
		 */
		base::Optional<ModuleID> m_repl_module_parent;

		/**
		 * The synthetic source file this chain link was created from.
		 * It plays the role of the main source file of a standard module.
		 */
		base::Optional<base::Ref<SourceFile>> m_synthetic_source_file;

		void makeAbstract() final {
			CORE_PANIC("makeAbstract called on SyntheticReplChainModuleTreeNode!");
		}

	protected:
		MRef<base::Optional<base::Ref<SourceFile>>> mainSourceFileSlot() override {
			return &m_synthetic_source_file;
		}

		MCRef<base::Optional<base::Ref<SourceFile>>> mainSourceFileSlot() const override {
			return &m_synthetic_source_file;
		}

	public:
		explicit SyntheticReplChainModuleTreeNode(base::Optional<ModuleID> repl_module_parent):
			  ModuleTree(),
			  m_repl_module_parent(repl_module_parent) {
			kind = ModuleKind::ReplChain;
		}

		base::Optional<ModuleID> getReplModuleParent() const override {
			return m_repl_module_parent;
		}

		[[nodiscard]]
		const base::Optional<base::Ref<SourceFile>>& getSyntheticSourceFile() const {
			return m_synthetic_source_file;
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

		void makeAbstract() final { CORE_PANIC("makeAbstract called on ScriptModuleTreeNode!"); }

	protected:
		std::vector<base::Ref<SourceFile>> collectOwnedSourceFiles() const override {
			std::vector<base::Ref<SourceFile>> source_files;
			for (const auto& chain_link: m_synthetic_repl_module_chain) {
				const auto& source_file = chain_link->getSyntheticSourceFile();
				if (source_file.has_value()) source_files.push_back(source_file.value());
			}
			return source_files;
		}

	public:
		ScriptModuleTreeNode(): ModuleTree() { kind = ModuleKind::Script; }
	};


}
