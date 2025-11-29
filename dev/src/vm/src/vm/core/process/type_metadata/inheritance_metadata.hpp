#pragma once

#include <base/collections/optional.hpp>
#include <base/collections/stable_hashmap.hpp>

#include <string_id/string_id.hpp>

#include <vm/core/process/type_metadata/definitions.hpp>

#include <unordered_set>
#include <variant>

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
		// Virtual method declarations for this class. Contains only methods introduced in this class.
		base::HashMap<base::StrID, TypeCRef> virtual_methods;
		// Contains all the implementations of virtual methods for this class/interface.
		// Unimplemented methods do not exist in the vtable.
		base::HashMap<base::StrID, base::StrID> vtable;
		// Cached all superclasses.
		std::unordered_set<TypeID> inherits_from;

		InheritanceMetadata(
			TypeCRef                                type,
			Kind                                    kind,
			std::vector<TypeCRef>                   implements,
			base::HashMap<base::StrID, TypeCRef>    virtual_methods,
			base::HashMap<base::StrID, base::StrID> vtable
		):
			  type{ type },
			  kind{ kind },
			  implements{ std::move(implements) },
			  virtual_methods{ std::move(virtual_methods) },
			  vtable{ std::move(vtable) } {}
	};
}
