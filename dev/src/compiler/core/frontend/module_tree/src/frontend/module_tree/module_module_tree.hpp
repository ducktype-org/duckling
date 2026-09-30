#pragma once

#include "module_tree.hpp"

namespace compiler::frontend {

	/**
	 * @brief Standard module node.
	 */
	class ModuleModuleTreeNode final: public ModuleTree {
		base::Optional<base::Ref<SourceFile>> m_main_source_file;

		base::HashMap<base::StrID, base::Ref<ModuleTree>> m_submodules;

		/**
		 * Other files in the module (not SourceFiles) currently nothing is
		 * happening with them. Do not use this in query unless AccessLocked is
		 * implemented for this
		 */
		base::HashMap<base::StrID, std::vector<fs::File>> m_other_files;

		void makeAbstract() final { CORE_PANIC("makeAbstract called on ModuleModuleTreeNode!"); }

		friend class ModuleTreeModifier;

	protected:
		MRef<base::Optional<base::Ref<SourceFile>>> mainSourceFileSlot() override {
			return &m_main_source_file;
		}

		MCRef<base::Optional<base::Ref<SourceFile>>> mainSourceFileSlot() const override {
			return &m_main_source_file;
		}

		MRef<base::HashMap<base::StrID, base::Ref<ModuleTree>>> submodulesSlot() override {
			return &m_submodules;
		}

		MCRef<base::HashMap<base::StrID, base::Ref<ModuleTree>>> submodulesSlot() const override {
			return &m_submodules;
		}

		MRef<base::HashMap<base::StrID, std::vector<fs::File>>> otherFilesSlot() override {
			return &m_other_files;
		}

		MCRef<base::HashMap<base::StrID, std::vector<fs::File>>> otherFilesSlot() const override {
			return &m_other_files;
		}

		std::vector<base::Ref<ModuleTree>> collectChildrenModules() const override {
			std::vector<base::Ref<ModuleTree>> children;
			for (auto& [_, submodule] : m_submodules) {
				children.push_back(submodule);
			}
			return children;
		}

	public:
		ModuleModuleTreeNode(): ModuleTree() { kind = ModuleKind::Module; }


		SubmodulesAccessLocked getSubmodules() const override {
			std::vector<ModuleAccessLocked> submodules;
			submodules.reserve(m_submodules.size());
			for (const auto& [name, submodule]: m_submodules)
				submodules.emplace_back(submodule->getModuleID());
			return { getModuleID(), std::move(submodules) };
		}

		ModuleChildAccessLocked getSubmoduleByName(base::StrID name) const override {
			base::Optional<ModuleID> child;
			if (auto it = m_submodules.find(name); it != m_submodules.end())
				child = it->second->getModuleID();
			return ModuleChildAccessLocked(getModuleID(), name, child);
		}
	};


}
