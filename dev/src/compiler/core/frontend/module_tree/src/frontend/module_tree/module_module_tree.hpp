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

		void makeAbstract() final;

		friend class ModuleTreeModifier;

	protected:
		MRef<base::Optional<base::Ref<SourceFile>>> mainSourceFileSlot() override;

		MCRef<base::Optional<base::Ref<SourceFile>>> mainSourceFileSlot() const override;

		MRef<base::HashMap<base::StrID, base::Ref<ModuleTree>>> submodulesSlot() override;

		MCRef<base::HashMap<base::StrID, base::Ref<ModuleTree>>> submodulesSlot() const override;

		MRef<base::HashMap<base::StrID, std::vector<fs::File>>> otherFilesSlot() override;

		MCRef<base::HashMap<base::StrID, std::vector<fs::File>>> otherFilesSlot() const override;

		std::vector<base::Ref<ModuleTree>> collectChildrenModules() const override;

	public:
		ModuleModuleTreeNode();

		SubmodulesAccessLocked getSubmodules() const override;

		ModuleChildAccessLocked getSubmoduleByName(base::StrID name) const override;
	};
}
