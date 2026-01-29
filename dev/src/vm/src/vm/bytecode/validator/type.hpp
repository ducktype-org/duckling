#pragma once

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/comptime/type_traits.hpp>
#include <base/extend_cpp/strongly_typed_id.hpp>
#include <base/types/bits_and_bytes.hpp>

#include <string_id/string_id.hpp>

#include <vm/bytecode/type_of_data.hpp>

#include <unordered_set>
#include <variant>

namespace vm::code::type {
	using TypeID = usize;

	class Type;
	using TypeCRef = base::CRef<Type>;

	namespace kind {
		struct Primitive;
		struct Pointer;
		struct FixedSizeTable;
		struct DynamicTable;
		struct Data;
		struct DataField;
		struct Variant;
		struct Function;
		struct Opaque;
	}

	template<class T>
	concept InnerType = base::IsOneOf<
		T,
		kind::Primitive,
		kind::Pointer,
		kind::FixedSizeTable,
		kind::DynamicTable,
		kind::Data,
		kind::Variant,
		kind::Function,
		kind::Opaque>;

	class Type {
	public:
		template<class T>
		Type(base::StrID name, TypeID id, T inner_type):
			  name(name),
			  id(id),
			  inner(std::move(inner_type)) {}

		[[nodiscard]] base::StrID getName() const { return name; }

		template<InnerType T>
		[[nodiscard]]
		const T& get() const {
			return std::get<T>(inner);
		}

		template<InnerType T>
		[[nodiscard]]
		bool is() const {
			return std::holds_alternative<T>(inner);
		}

		bool operator==(const Type& other) const { return other.id == id; }

		[[nodiscard]]
		Bytes getSize() const {
			CORE_ASSERT(size != TypeSize(-1), "getSize called before type finalization");
			return size;
		}

		/**
		 * Get inner type of pointer, fixed size or dynamic table
		 * @return Some(inner type) for pointer, fixed size or dynamic table. none otherwise
		 */
		base::Optional<TypeCRef> getInnerType() const;

		[[nodiscard]]
		bool isTriviallyCopyable() const;

		// data
		[[nodiscard]]
		base::Optional<Bytes> getFieldOffsetByName(base::StrID field_name) const;
		[[nodiscard]]
		base::Optional<base::CRef<std::vector<kind::DataField>>> getFields() const;

		// variant
		base::Optional<usize>                 getTypeTagSizeBytes() const;
		base::Optional<std::vector<TypeCRef>> getVariantAlternatives() const;

		// inheritance
		[[nodiscard]]
		base::Optional<base::CRef<InheritanceMetadata>> getInheritanceMetadata() const;
		[[nodiscard]]
		base::Optional<TypeCRef> getSuperClass() const;
		[[nodiscard]]
		bool inheritsFrom(TypeCRef other) const;
		[[nodiscard]]
		bool isInstantiable() const;

		// function
		[[nodiscard]]
		base::Optional<u64> getParameterCount() const;
		[[nodiscard]]
		base::Optional<u64> getParametersSize() const;
		[[nodiscard]]
		base::Optional<TypeCRef> getNthParameterType(u64 parameter_id) const;
		[[nodiscard]]
		base::Optional<TypeCRef> getResultType() const;

	private:
		base::StrID name;
		TypeID      id;

		Inner inner;
	};

	namespace kind {
		struct Primitive {
			usize size;
		};

		struct Pointer {
			TypeRef inner;

			Pointer(TypeRef inner): inner(inner) {}
		};

		struct FixedSizeTable {
			TypeRef inner;
			usize   table_size;
		};

		struct DynamicTable {
			TypeRef inner;
		};

		/**
		 * @brief A field inside a Data type.
		 */
		struct DataField {
			STRONG_TYPEDEF_ID_DIRECT_CREATION(FieldID);
			Bytes   offset;
			TypeRef type;
		};

		/**
		 * @brief Struct/Class (data) type representation.
		 * If declared as a class contains a vtable field at the front of field vector.
		 */
		struct Data {
			base::HashMap<base::StrID, FieldID> field_name_map;
			std::vector<DataField>              fields;

			// All the types this type directly or indirectly inherits from.
			std::unordered_set<TypeRef> super_types;

			// In a class-like way. Only one superclass allowed
			base::Optional<TypeRef> extends;

			// In a interface-like way. Multiple interfaces allowed
			std::unordered_set<TypeRef> implements;

			bool is_abstract = false;
		};

		struct Variant {
			usize                type_tag_size_bytes;
			std::vector<TypeRef> alternatives;
		};

		struct Function {
			std::vector<TypeRef> parameters;
			TypeRef              result;
		};

		struct Opaque {
			usize size;
		};
	}

}
