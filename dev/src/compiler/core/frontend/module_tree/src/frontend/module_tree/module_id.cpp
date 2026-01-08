#include "module_id.hpp"

#include <base/config/build_type.hpp>
#ifdef BUILD_TYPE_DEV
	#include "module_tree.hpp"
#endif

namespace compiler::frontend {
	void ModuleID::checkDanglingReference() const {
		IF_BUILD_TYPE_DEV(ModuleTree::checkDanglingReference(ref));
	}
}
