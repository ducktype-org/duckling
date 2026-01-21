#include "module_id.hpp"

#include "module_tree.hpp"

#include <base/config/build_type.hpp>

namespace compiler::frontend {
	void ModuleID::checkDanglingReference() const {
		IF_BUILD_TYPE_DEV(ModuleTree::checkDanglingReference(ref));
	}
}
