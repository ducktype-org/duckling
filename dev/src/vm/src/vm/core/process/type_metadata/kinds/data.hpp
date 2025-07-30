#pragma once

#include "../definitions.hpp"
#include "../inheritance_metadata.hpp"

#include <base/maps.hpp>
#include <base/optional.hpp>
#include <base/string_id.hpp>
#include <base/strongly_typed_id.hpp>

#include <vector>

namespace vm::kind {

	struct FieldDesc {
		Offset  offset;
		TypeRef type;
	};

	struct Data {
		STRONG_TYPEDEF_ID_DIRECT_CREATION(FieldID)

		base::HashMap<base::StrID, FieldID> field_name_map;
		std::vector<FieldDesc>              fields;

		base::Optional<InheritanceMetadata> inheritance_metadata;
	};
}
