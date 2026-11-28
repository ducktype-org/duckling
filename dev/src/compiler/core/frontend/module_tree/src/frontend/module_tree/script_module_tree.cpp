// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "script_module_tree.hpp"

namespace compiler::frontend {
	std::vector<base::Ref<ModuleTree>> SyntheticReplChainModuleTreeNode::collectChildrenModules(
	) const {
		// @TODO: #4253 bring this back maybe!
		// CORE_ASSERT(
		// 	!query::Context::areWeInsideQuery(), "collectChildrenModules called from within query!"
		// );

		std::vector<base::Ref<ModuleTree>> children;
		if (m_repl_module_child.has_value()) children.emplace_back(m_repl_module_child.value());
		return children;
	}

	std::vector<base::Ref<SourceFile>> SyntheticReplChainModuleTreeNode::collectOwnedSourceFiles(
	) const {
		std::vector<base::Ref<SourceFile>> source_files;
		if (m_synthetic_source_file.has_value())
			source_files.push_back(m_synthetic_source_file.value());
		return source_files;
	}

	SyntheticReplChainModuleTreeNode::SyntheticReplChainModuleTreeNode(): ModuleTree() {
		kind = ModuleKind::ReplChain;
	}

	const base::Optional<base::Ref<SourceFile>>& SyntheticReplChainModuleTreeNode::getSyntheticSourceFile(
	) const {
		return m_synthetic_source_file;
	}

	void SyntheticReplChainModuleTreeNode::addKindSpecificDataToHash(
		hashing::ComponentHash::HashAlg& partial
	) const {
		const bool parent_is_chain_node
			= m_parent.has_value() and m_parent.value()->getKind() == ModuleKind::ReplChain;
		hashing::addToHash(partial, parent_is_chain_node);
		if (parent_is_chain_node)
			hashing::addToHash(partial, ModuleTree::getModuleHash(m_parent.value()->getModuleID()));
	}

	ScriptModuleTreeNode::ScriptModuleTreeNode(): ModuleTree() { kind = ModuleKind::Script; }

	std::vector<base::Ref<ModuleTree>> ScriptModuleTreeNode::collectChildrenModules() const {
		CORE_ASSERT(
			!query::Context::areWeInsideQuery(), "collectChildrenModules called from within query!"
		);

		std::vector<base::Ref<ModuleTree>> children;
		if (m_repl_module_child.has_value()) children.emplace_back(m_repl_module_child.value());
		return children;
	}

	std::vector<base::Ref<SourceFile>> ScriptModuleTreeNode::collectOwnedSourceFiles() const {
		return {};
	}

	void ScriptModuleTreeNode::addKindSpecificDataToHash(hashing::ComponentHash::HashAlg&) const {}
}
