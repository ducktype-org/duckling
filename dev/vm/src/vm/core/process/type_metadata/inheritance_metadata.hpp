#pragma once

#include "base/stable_hashmap.hpp"
#include <base/maps.hpp>
#include <base/optional.hpp>
#include <base/string_id.hpp>

#include <vm/core/process/type_metadata/definitions.hpp>

#include <variant>
#include <vector>

namespace vm {

	// using ActualVmFunctionPointer = void*;

	// struct VTable {
	// 	// Pointers to virtual methods implementation.
	// 	std::vector<ActualVmFunctionPointer> vtable;

	// 	base::HashMap<std::pair<TypeCRef, base::StrID>, base::StrID> method_to_vtable_index;
	// };

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
		// Virtual Methods to actual implementations for this class.
		base::HashMap<base::StrID, TypeCRef> vmethods_implementations;

		// base::Optional<VTable> vtable_data;  // Exists only for classes with virtual methods.
		// base::HashMap<std::pair<TypeCRef, base::StrID>, ActualVmFunctionPointer>
		// 	method_implementations;
		// 	TODO: Check if implementations have the same type as virtual methods declarations.

		InheritanceMetadata(
			TypeCRef                             type,
			Kind                                 kind,
			std::vector<TypeCRef>                implements,
			base::HashMap<base::StrID, TypeCRef> virtual_methods,
			base::HashMap<base::StrID, TypeCRef> vmethods_implementations = {}
		):
			  type{ type },
			  kind{ kind },
			  implements{ std::move(implements) },
			  virtual_methods{ std::move(virtual_methods) },
			  vmethods_implementations{ std::move(vmethods_implementations) } {}
	};
}
