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

		friend class ModuleTreeModifier;

	protected:
		Ref<base::Optional<base::Ref<SourceFile>>> mainSourceFileSlot() override;

		CRef<base::Optional<base::Ref<SourceFile>>> mainSourceFileSlot() const override;

		Ref<base::HashMap<base::StrID, base::Ref<ModuleTree>>> submodulesSlot() override;

		CRef<base::HashMap<base::StrID, base::Ref<ModuleTree>>> submodulesSlot() const override;

		Ref<base::HashMap<base::StrID, std::vector<fs::File>>> otherFilesSlot() override;

		CRef<base::HashMap<base::StrID, std::vector<fs::File>>> otherFilesSlot() const override;

		std::vector<base::Ref<ModuleTree>> collectChildrenModules() const override;

		const base::HashMap<base::StrID, std::vector<fs::File>>& getOtherFiles() const override;

	public:
		ModuleModuleTreeNode();

		SubmodulesAccessLocked getSubmodules() const override;

		ModuleChildAccessLocked getSubmoduleByName(base::StrID name) const override;
	};
}
