#pragma once

#include <base/collections/stable_container.hpp>
#include <base/extend_cpp/strongly_typed_id.hpp>
#include <base/types/bits_and_bytes.hpp>
#include <base/types/ints.hpp>

namespace vm {
	/// Size of type in bytes
	using TypeSize = Bytes;

	using Offset = Bytes;
    using ShadowOffset = u32;

	class Type;

	using TypeRef  = Ref<Type>;
	using TypeCRef = CRef<Type>;

	STRONG_TYPEDEF_ID_DIRECT_CREATION(TypeID);
	STRONG_TYPEDEF_ID_DIRECT_CREATION(GlobalDataID);
}

ID_STD_HASH(vm::TypeID);
ID_STD_HASH(vm::GlobalDataID);

namespace std {
	template<>
	struct hash<vm::TypeCRef> {
		size_t operator()(const vm::TypeCRef& type_ref) const noexcept {
			return usize(type_ref.get());
		}
	};
}
