#pragma once

#include <base/maps.hpp>
#include <base/optional.hpp>
#include <base/string_id.hpp>

#include <vm/core/process/type_metadata/definitions.hpp>

#include <variant>

namespace vm {
	struct VTable {
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

		VTable(
			TypeCRef                             type,
			Kind                                 kind,
			std::vector<TypeCRef>                implements,
			base::HashMap<base::StrID, TypeCRef> virtual_methods
		):
			  type{ type },
			  kind{ kind },
			  implements{ std::move(implements) },
			  virtual_methods{ std::move(virtual_methods) } {}

		static VTable forClass(
			TypeCRef                             type,
			bool                                 is_abstract,
			base::Optional<TypeCRef>             extends,
			std::vector<TypeCRef>                implements,
			base::HashMap<base::StrID, TypeCRef> virtual_methods
		) {
			return VTable(
				type,
				Class{ .is_abstract = is_abstract, .extends = extends },
				std::move(implements),
				std::move(virtual_methods)
			);
		}

		static VTable forInterface(
			TypeCRef                             type,
			std::vector<TypeCRef>                implements,
			base::HashMap<base::StrID, TypeCRef> virtual_methods
		) {
			return VTable(type, Interface{}, std::move(implements), std::move(virtual_methods));
		}
	};
}
