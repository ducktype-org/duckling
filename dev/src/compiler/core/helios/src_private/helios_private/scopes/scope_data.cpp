// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
