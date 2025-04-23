#pragma once

#include <base/maps.hpp>
#include <base/optional.hpp>
#include <base/string_id.hpp>
#include <base/variant.hpp>

#include <vm/core/process/type_metadata/definitions.hpp>

#include <variant>

namespace vm {
	struct InheritanceMetadata {
		struct Class {
			bool                     is_abstract;
			base::Optional<TypeCRef> extends;
		};

		struct Interface {};

		using Kind = std::variant<Interface, Class>;

		TypeCRef                             type;
		Kind                                 kind;
		std::vector<TypeCRef>                implements;
		base::HashMap<base::StrID, TypeCRef> virtual_methods;

		InheritanceMetadata(
			TypeCRef                             type,
			Kind                                 kind,
			std::vector<TypeCRef>                implements,
			base::HashMap<base::StrID, TypeCRef> virtual_methods
		):
			  type{ type },
			  kind{ kind },
			  implements{ std::move(implements) },
			  virtual_methods{ std::move(virtual_methods) } {}
	};
}
