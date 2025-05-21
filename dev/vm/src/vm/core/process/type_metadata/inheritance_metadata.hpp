#pragma once

#include "base/stable_hashmap.hpp"
#include <base/maps.hpp>
#include <base/optional.hpp>
#include <base/string_id.hpp>

#include <vm/core/process/type_metadata/definitions.hpp>

#include <variant>
#include <vector>

namespace vm {
	struct InheritanceMetadata {
		struct Class {
			bool                     is_abstract;
			base::Optional<TypeCRef> extends;
		};

		struct Interface {};

		using Kind = std::variant<Interface, Class>;

		TypeCRef              type;
		Kind                  kind;
		std::vector<TypeCRef> implements;
		// Virtual method declarations for this class. Contains only method introduced in this class.
		base::HashMap<base::StrID, TypeCRef> virtual_methods;
		base::HashMap<base::StrID, TypeCRef> implementations; // TODO: This may be not needed here.
		// This contains all the actual implementations of virtual methods for this class/interface.
		// VWe call it a VTable, tho it's more of a VMap. Creating a table is causes problem when
		// working with multiple inheritance of interfaces.
		base::HashMap<base::StrID, TypeCRef> vtable;

		// 	TODO: Check if implementations have the same type as virtual methods declarations.
		// 	TODO: VTable should only exist for instantiable classes. Make it an optional.

		InheritanceMetadata(
			TypeCRef                             type,
			Kind                                 kind,
			std::vector<TypeCRef>                implements,
			base::HashMap<base::StrID, TypeCRef> virtual_methods,
			base::HashMap<base::StrID, TypeCRef> implementations,
			base::HashMap<base::StrID, TypeCRef> vtable = {}
		):
			  type{ type },
			  kind{ kind },
			  implements{ std::move(implements) },
			  virtual_methods{ std::move(virtual_methods) },
			  implementations{ std::move(implementations) },
			  vtable{ std::move(vtable) } {}
	};
}
