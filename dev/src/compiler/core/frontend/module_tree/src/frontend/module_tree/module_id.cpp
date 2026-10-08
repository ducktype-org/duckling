// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "module_id.hpp"

#include "module_tree.hpp"

#include <base/config/build_type.hpp>

namespace compiler::frontend {
	ModuleID::ModuleID(base::Ref<ModuleTree> ref):
		  hash([ref] {
			  ref->updateModuleHashFromRootToThis();
			  return ref->m_hash.value();
		  }()) {}

	base::Ref<ModuleTree> ModuleID::resolve() const {
		auto ref = ModuleTree::getRegisteredModule(hash);
		IF_BUILD_TYPE_DEV(ModuleTree::checkDanglingReference(ref));
		return ref;
	}
}
