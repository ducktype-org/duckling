#include "module_module_tree.hpp"

namespace compiler::frontend {
	void ModuleModuleTreeNode::makeAbstract() {
		CORE_PANIC("makeAbstract called on ModuleModuleTreeNode!");
	}

	Ref<base::Optional<base::Ref<SourceFile>>> ModuleModuleTreeNode::mainSourceFileSlot() {
		return &m_main_source_file;
	}

	CRef<base::Optional<base::Ref<SourceFile>>> ModuleModuleTreeNode::mainSourceFileSlot() const {
		return &m_main_source_file;
	}

	Ref<base::HashMap<base::StrID, base::Ref<ModuleTree>>> ModuleModuleTreeNode::submodulesSlot() {
		return &m_submodules;
	}

	CRef<base::HashMap<base::StrID, base::Ref<ModuleTree>>> ModuleModuleTreeNode::submodulesSlot() const {
		return &m_submodules;
	}

	Ref<base::HashMap<base::StrID, std::vector<fs::File>>> ModuleModuleTreeNode::otherFilesSlot() {
		return &m_other_files;
	}

	CRef<base::HashMap<base::StrID, std::vector<fs::File>>> ModuleModuleTreeNode::otherFilesSlot() const {
		return &m_other_files;
	}

	const base::HashMap<base::StrID, std::vector<fs::File>>& ModuleModuleTreeNode::getOtherFiles() const {
		return m_other_files;
	}

	std::vector<base::Ref<ModuleTree>> ModuleModuleTreeNode::collectChildrenModules() const {
		std::vector<base::Ref<ModuleTree>> children;
		for (auto& [_, submodule]: m_submodules) children.push_back(submodule);
		return children;
	}

	ModuleModuleTreeNode::ModuleModuleTreeNode(): ModuleTree() { kind = ModuleKind::Module; }

	SubmodulesAccessLocked ModuleModuleTreeNode::getSubmodules() const {
		std::vector<ModuleAccessLocked> submodules;
		submodules.reserve(m_submodules.size());
		for (const auto& [name, submodule]: m_submodules)
			submodules.emplace_back(submodule->getModuleID());
		return { getModuleID(), std::move(submodules) };
	}

	ModuleChildAccessLocked ModuleModuleTreeNode::getSubmoduleByName(base::StrID name) const {
		base::Optional<ModuleID> child;
		if (auto it = m_submodules.find(name); it != m_submodules.end())
			child = it->second->getModuleID();
		return ModuleChildAccessLocked(getModuleID(), name, child);
	}
}
