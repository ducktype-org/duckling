#pragma once

#include "../definitions.hpp"

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

		// @todo when hashmap has operator = change to base::HashMap
		std::unordered_map<base::StrID, FieldID> field_name_map;
		std::vector<FieldDesc>                   fields;
	};
}
