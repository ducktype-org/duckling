#pragma once

#include "../definitions.hpp"
#include "../inheritance_metadata.hpp"

#include <base/misc/optional.hpp>
#include <base/string_id.hpp>

#include <unordered_map>
#include <vector>

namespace vm::kind {

	struct FieldDesc {
		Offset  offset;
		TypeRef type;
	};

	struct Data {
		// @todo: change to strongly typed when it will be in utils
		using FieldID = u64;

		base::HashMap<base::StrID, FieldID> field_name_map;
		std::vector<FieldDesc>              fields;

		base::Optional<InheritanceMetadata> inheritance_metadata;
	};
}
