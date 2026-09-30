#include "script_module_tree.hpp"

namespace compiler::frontend {
	void SyntheticReplChainModuleTreeNode::makeAbstract() {
		CORE_PANIC("makeAbstract called on SyntheticReplChainModuleTreeNode!");
	}

	Ref<base::Optional<base::Ref<SourceFile>>> SyntheticReplChainModuleTreeNode::mainSourceFileSlot() {
		return &m_synthetic_source_file;
	}

	CRef<base::Optional<base::Ref<SourceFile>>> SyntheticReplChainModuleTreeNode::mainSourceFileSlot() const {
		return &m_synthetic_source_file;
	}

	std::vector<base::Ref<ModuleTree>> SyntheticReplChainModuleTreeNode::collectChildrenModules() const {
		std::vector<base::Ref<ModuleTree>> children;
		if (m_repl_module_child.has_value()) children.emplace_back(m_repl_module_child.value());
		return children;
	}

	SyntheticReplChainModuleTreeNode::SyntheticReplChainModuleTreeNode(): ModuleTree() {
		kind = ModuleKind::ReplChain;
	}

	const base::Optional<base::Ref<SourceFile>>& SyntheticReplChainModuleTreeNode::getSyntheticSourceFile() const {
		return m_synthetic_source_file;
	}

	void ScriptModuleTreeNode::makeAbstract() {
		CORE_PANIC("makeAbstract called on ScriptModuleTreeNode!");
	}

	ScriptModuleTreeNode::ScriptModuleTreeNode(): ModuleTree() { kind = ModuleKind::Script; }

	std::vector<base::Ref<ModuleTree>> ScriptModuleTreeNode::collectChildrenModules() const {
		std::vector<base::Ref<ModuleTree>> children;
		if (m_repl_module_child.has_value()) children.emplace_back(m_repl_module_child.value());
		return children;
	}
}
