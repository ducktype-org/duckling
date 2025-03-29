#pragma once

#include <unordered_map>
#include <vector>
#include <base/string_id.hpp>
#include "../definitions.hpp"
#include <base/optional.hpp>
#include "../vtable.hpp"

namespace vm::kind {

	struct FieldDesc {
		Offset  offset;
		TypeRef type;
	};

	struct Data {
		// @todo: change to strongly typed when it will be in utils
		using FieldID = u64;

		// @todo when hashmap has operator = change to base::HashMap
		std::unordered_map<base::StrID, FieldID> field_name_map;
		std::vector<FieldDesc>                   fields;

		base::Optional<VTable> vtable;
	};
}
