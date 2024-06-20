#pragma once

#include <variant>
#include <base/string_id.hpp>
#include <base/optional.hpp>
#include <memory_data/pointer.hpp>
#include "kinds.hpp"

#include <nlohmann/json.hpp>

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
		[[nodiscard]]
		inline TypeId getId() const {
			return id;
		}

		[[nodiscard]]
		inline base::StrId getName() const {
			return name;
		}

		[[nodiscard]]
		inline TypeSize getSize() const {
			RIFT_ASSERT(size != TypeSize(-1), "getSize called before type finalization");
			return size;
		}

		template<class T>
		base::Optional<const T&> get() const {
			if (std::holds_alternative<T>(kind)) return std::get<T>(kind);
			return {};
		}

		[[nodiscard]]
		inline Kind getKind() const {
			return kind_type;
		}

		[[nodiscard]]
		inline bool isPrimitive(TypeSize qsize) const {
			return getKind() == Kind::Primitive and getSize() == qsize;
		}

		base::Optional<TypeCRef> getLowestTypeAtPos(Offset pos) const;


		// @todo: Interface below may change

		// @TODO: move function below to kind:: structures without `option`
		// Forward here version with option

		/**
		 * Get inner type of pointer, static or dynamic table
		 * @return some(inner type) for pointer, static or dynamic table. none otherwise
		 */
		base::Optional<TypeCRef> getInnerType() const;

		// staticTable
		base::Optional<u64> getStaticTableSize() const;

		// data
		base::Optional<TypeCRef> getFieldType(kind::Data::FieldId fieldId) const;
		base::Optional<Offset>   getFieldOffset(kind::Data::FieldId fieldId) const;
		base::Optional<TypeCRef> getFieldTypeByOffset(Offset offset) const;
		base::Optional<TypeCRef> getFieldTypeByOffsetRecursive(Offset offset) const;

		// variant
		base::Optional<u64>      getVariantCount() const;
		base::Optional<TypeCRef> getNthVariantType(u64 variantId) const;

		// function
		base::Optional<u64>      getParameterCount() const;
		base::Optional<TypeCRef> getNthParameterType(u64 parameterId) const;
		base::Optional<TypeCRef> getResultType() const;

		friend class TypeMetadata;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(Type, size);  // TODO: add better output of type
	};
}

// REGISTER_PARSE_TYPE_ALIAS(vm::Type, "Type");
