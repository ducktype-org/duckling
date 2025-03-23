#pragma once

#include <unordered_map>
#include <variant>
#include <vector>
#include <base/string_id.hpp>
#include "../definitions.hpp"

namespace vm::kind {

	struct FieldDesc {
		Offset  offset;
		TypeRef type;
	};

	namespace inheritance {
		struct Class {
			base::Optional<TypeRef>             extends;
			std::vector<TypeRef>                implements;
			base::HashMap<base::StrID, TypeRef> virtual_methods;
		};

		struct Interface {
			std::vector<TypeRef>                implements;
			base::HashMap<base::StrID, TypeRef> virtual_methods;
		};

		/// A marker that this Data does not represent a class nor an interface.
		struct Plain {};

		using Role = std::variant<Class, Interface, Plain>;
	}

	struct Data {
		// @todo: change to strongly typed when it will be in utils
		using FieldID = u64;

		// @todo when hashmap has operator = change to base::HashMap
		std::unordered_map<base::StrID, FieldID> field_name_map;
		std::vector<FieldDesc>                   fields;

		inheritance::Role inheritance_role;
	};
}
