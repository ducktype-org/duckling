#pragma once

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>

#include <string_id/string_id.hpp>

#include <vm/core/safe/type_metadata/definitions.hpp>

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

		TypeCRef                     type;
		Kind                         kind;
		std::unordered_set<TypeCRef> implements;
		// Virtual method declarations for this class.
		// This serves as an interface.
		base::HashMap<base::StrID, TypeCRef> available_methods;
		// Contains all the implementations of virtual methods for this class/interface.
		// Unimplemented methods do not exist in the vtable.
		base::HashMap<base::StrID, base::StrID> vtable;
		// Cached all superclasses.
		std::unordered_set<TypeID> inherits_from;

		InheritanceMetadata(
			TypeCRef                                type,
			Kind                                    kind,
			std::unordered_set<TypeCRef>            implements,
			base::HashMap<base::StrID, TypeCRef>    available_methods,
			base::HashMap<base::StrID, base::StrID> vtable
		):
			  type{ type },
			  kind{ kind },
			  implements{ std::move(implements) },
			  available_methods{ std::move(available_methods) },
			  vtable{ std::move(vtable) } {}
	};
}
