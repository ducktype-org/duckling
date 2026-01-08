#include <base/config/build_type.hpp>

#include "module_id.hpp"
#ifdef BUILD_TYPE_DEV
	#include "module_tree.hpp"
#endif

namespace compiler::frontend {
	void ModuleID::checkDanglingReference() const {
		IF_BUILD_TYPE_DEV(ModuleTree::checkDanglingReference(ref));
	}
}
