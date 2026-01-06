#include "scope_data.hpp"

namespace compiler::helios {

	ScopeData ScopeData::perfectClone() const {
		ScopeData out(parent, is_root, related_pst_element, parent_module, depth);

		// copy unstable id as well (which is normally not possible):
		out.unstable_id = unstable_id;

		return out;
	}

}
