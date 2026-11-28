// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "standard_module_tree.hpp"

namespace compiler::frontend {
	const base::HashMap<base::StrID, std::vector<fs::File>>& StandardModuleTreeNode::getOtherFiles(
	) const {
		return m_other_files;
	}

	std::vector<base::Ref<ModuleTree>> StandardModuleTreeNode::collectChildrenModules() const {
		// @TODO: #4253 if this hits, feel free to comment it out, we will deal with it in #4253!
		CORE_ASSERT(
			!query::Context::areWeInsideQuery(), "collectChildrenModules called from within query!"
		);

		std::vector<base::Ref<ModuleTree>> children;
		for (auto& [_, submodule]: m_submodules) children.push_back(submodule);
		return children;
	}

	std::vector<base::Ref<SourceFile>> StandardModuleTreeNode::collectOwnedSourceFiles() const {
		std::vector<base::Ref<SourceFile>> source_files;
		if (m_main_source_file.has_value()) source_files.push_back(m_main_source_file.value());
		return source_files;
	}

	void StandardModuleTreeNode::addKindSpecificDataToHash(hashing::ComponentHash::HashAlg&) const {
	}

	StandardModuleTreeNode::StandardModuleTreeNode(): ModuleTree() { kind = ModuleKind::Module; }

	SubmodulesAccessLocked StandardModuleTreeNode::getSubmodules() const {
		std::vector<ModuleAccessLocked> submodules;
		submodules.reserve(m_submodules.size());
		for (const auto& [name, submodule]: m_submodules)
			submodules.emplace_back(submodule->getModuleID());
		return { getModuleID(), std::move(submodules) };
	}

	ModuleChildAccessLocked StandardModuleTreeNode::getSubmoduleByName(base::StrID name) const {
		base::Optional<ModuleID> child;
		if (auto it = m_submodules.find(name); it != m_submodules.end())
			child = it->value->getModuleID();
		return ModuleChildAccessLocked(getModuleID(), name, child);
	}
}
