#pragma once

#include <base/maps.hpp>
#include <base/optional.hpp>
#include <base/string_id.hpp>

#include <vm/core/process/type_metadata/definitions.hpp>

#include <variant>

namespace vm {
	struct VTable {
		struct Class {
			base::Optional<TypeRef> extends;
		};

		struct Interface {};

		using Kind = std::variant<Interface, Class>;

		Kind                                kind;
		std::vector<TypeRef>                implements;
		base::HashMap<base::StrID, TypeRef> virtual_methods;
	};
}
