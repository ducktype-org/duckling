#include "scope_data.hpp"

namespace compiler::helios {
	ScopeData ScopeData::perfectClone() const {
		ScopeData out(parent, is_root, related_pst_element_hash, parent_module, depth);

		// copy unstable id as well (which is normally not possible):
		out.unstable_id = unstable_id;

		return out;
	}

	base::Optional<pst::AccessLocked<pst::LangElement>> ScopeData::relatedPSTElement() const {
		if (related_pst_element_hash.has_value())
			return pst::LangElement::getByStableHash(related_pst_element_hash.value());
		else
			return std::nullopt;
	}
}
