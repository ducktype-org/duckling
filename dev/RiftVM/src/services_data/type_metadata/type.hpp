#pragma once

#include "kinds.hpp"
#include <base/option.hpp>
#include <base/string_id.hpp>
#include <memory_data/pointer.hpp>
#include <variant>

namespace vm {
	class TypeMetadata;

	/// Size of type in bytes
	// @TODO: change to strongly typed int
	using TypeSize = u64;

	class Type {
	public:
		constexpr static TypeSize PointerSize = sizeof(Pointer);

		enum class Kind {
			None,
			Primitive,
			Pointer,
			StaticTable,
			DynamicTable,
			Data,
			Variant,
			Function
		};

	private:
		enum class State { Declared, Defined, Finalizing, Finalized };

		State state = State::Declared;

		base::StrId name;
		TypeSize    size      = TypeSize(-1);
		Kind        kind_type = Kind::None;
		TypeId      id;

		std::variant<
			std::monostate,
			kind::Primitive,
			kind::Pointer,
			kind::StaticTable,
			kind::DynamicTable,
			kind::Data,
			kind::Variant,
			kind::Function>
			kind;

		Type() = default;

	public:
		// Type declaration:
		static Type declareType(base::StrId name);

		// Type definition:
		void definePrimitive(TypeSize size);
		void definePointer(TypeCRef inner);
		void defineStaticTable(TypeRef inner, u64 table_size);
		void defineDynamicTable(TypeRef inner);
		void defineData(const std::vector<std::pair<base::StrId, TypeRef>>& fields_definitions);
		void defineVariant(const std::vector<TypeRef>& variants_definitions);
		void defineFunction(std::vector<TypeCRef> parameters, TypeCRef result);

		// Type finalization:
		void finalize();

		// Type query:
		TypeId      getId() const;
		base::StrId getName() const;
		TypeSize    getSize() const;

		template<class T>
		option<const T&> get() const {
			if (std::holds_alternative<T>(kind)) return some<const T&>(std::get<T>(kind));
			return none<const T&>();
		}

		Kind getKind() const;

		bool isPrimitive(TypeSize size) const;

		option<TypeCRef> getLowestTypeAtPos(Offset pos) const;


		// @todo: Interface below may change

		// @TODO: move function below to kind:: structures without `option`
		// Forward here version with option

		/**
		 * Get inner type of pointer, static or dynamic table
		 * @return some(inner type) for pointer, static or dynamic table. none otherwise
		 */
		option<TypeCRef> getInnerType() const;

		// staticTable
		option<u64> getStaticTableSize() const;

		// data
		option<TypeCRef> getFieldType(kind::Data::FieldId fieldId) const;
		option<Offset>   getFieldOffset(kind::Data::FieldId fieldId) const;
		option<TypeCRef> getFieldTypeByOffset(Offset offset) const;
		option<TypeCRef> getFieldTypeByOffsetRecursive(Offset offset) const;

		// variant
		option<u64>      getVariantCount() const;
		option<TypeCRef> getNthVariantType(u64 variantId) const;

		// function
		option<u64>      getParameterCount() const;
		option<TypeCRef> getNthParameterType(u64 parameterId) const;
		option<TypeCRef> getResultType() const;

		friend class TypeMetadata;

		JS_OBJ(size);  // TODO: add better output of type
	};
}

REGISTER_PARSE_TYPE_ALIAS(vm::Type, "Type");
