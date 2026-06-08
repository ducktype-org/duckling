#pragma once

#include "../definitions.hpp"
#include "../inheritance_metadata.hpp"

#include <base/collections/optional.hpp>

#include <string_id/string_id.hpp>

#include <vector>

namespace vm::kind {

	struct FieldDesc {
		Offset       offset;
		ShadowOffset shadow_offset;
		TypeRef      type;
	};

	struct Data {
		// @todo: change to strongly typed when it will be in utils
		using FieldID = u64;

		base::HashMap<base::StrID, FieldID> field_name_map;
		std::vector<FieldDesc>              fields;

		base::Optional<InheritanceMetadata> inheritance_metadata;
		std::vector<u32>                    byte_to_shadow;
	};
}
