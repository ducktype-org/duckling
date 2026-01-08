#include "module_id.hpp"

#include <base/config/build_type.hpp>
#include "module_tree.hpp"

namespace compiler::frontend {
	void ModuleID::checkDanglingReference() const {
		IF_BUILD_TYPE_DEV(ModuleTree::checkDanglingReference(ref));
	}
}
