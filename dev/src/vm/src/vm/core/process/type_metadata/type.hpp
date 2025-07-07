#pragma once

#include "kinds.hpp"

#include <json/json.hpp>

#include <base/optional.hpp>
#include <base/string_id.hpp>

#include <vm/core/process/memory/pointer.hpp>

#include <variant>

namespace vm {
	class TypeMetadata;

	/// Size of type in bytes
	// @TODO: change to strongly typed int
	using TypeSize = u64;

	class Type final {
	public:
		constexpr static TypeSize POINTER_SIZE = sizeof(Pointer);

		enum class Kind {
			None,
			Primitive,
			Pointer,
			FixedSizeTable,
			DynamicTable,
			Data,
			Variant,
			Function,
			Opaque,
		};

	private:
		enum class State { Declared, Defined, Finalizing, Finalized };

		State state = State::Declared;

		base::StrID name;
		TypeSize    size      = TypeSize(-1);
		Kind        kind_type = Kind::None;
		TypeID      id{};

		std::variant<
			std::monostate,
			kind::Primitive,
			kind::Pointer,
			kind::FixedSizeTable,
			kind::DynamicTable,
			kind::Data,
			kind::Variant,
			kind::Function,
			kind::Opaque>
			kind;

		Type() = default;

	public:
		// Type declaration:
		static Type declareType(base::StrID name);

		// Type definition:
		void definePrimitive(TypeSize size);
		void definePointer(TypeCRef inner);
		void defineFixedSizeTable(TypeRef inner, u64 table_size);
		void defineDynamicTable(TypeRef inner);
		void defineData(
			const std::vector<std::pair<base::StrID, TypeRef>>& fields_definitions,
			base::Optional<InheritanceMetadata>                 inheritance_metadata
		);
		void defineVariant(const std::vector<TypeRef>& variants_definitions);
		void defineFunction(std::vector<TypeCRef> parameters, TypeCRef result);
		void defineOpaque(TypeSize size);

		// Type finalization:
		void finalize();

		// Type query:
		[[nodiscard]]
		TypeID getID() const {
			return id;
		}

		[[nodiscard]]
		base::StrID getName() const {
			return name;
		}

		[[nodiscard]]
		TypeSize getSize() const {
			CORE_ASSERT(size != TypeSize(-1), "getSize called before type finalization");
			return size;
		}

		template<class T>
		base::Optional<base::CRef<T>> get() const {
			if (std::holds_alternative<T>(kind)) return &std::get<T>(kind);
			return {};
		}

		[[nodiscard]]
		Kind getKind() const {
			return kind_type;
		}

		[[nodiscard]]
		bool isPrimitive(TypeSize qsize) const {
			return getKind() == Kind::Primitive and getSize() == qsize;
		}

		base::Optional<TypeCRef> getLowestTypeAtPos(Offset pos) const;


		// @todo: Interface below may change

		// @TODO: move function below to kind:: structures without `option`
		// Forward here version with option

		/**
		 * Get inner type of pointer, fixed size or dynamic table
		 * @return some(inner type) for pointer, fixed size or dynamic table. none otherwise
		 */
		base::Optional<TypeCRef> getInnerType() const;

		// fixedSizeTable
		base::Optional<u64> getFixedSizeTableSize() const;

		// data
		[[nodiscard]]
		base::Optional<usize> getFieldCount() const;
		[[nodiscard]]
		base::Optional<TypeCRef> getFieldType(kind::Data::FieldID field_id) const;
		[[nodiscard]]
		base::Optional<Offset> getFieldOffset(kind::Data::FieldID field_id) const;
		[[nodiscard]]
		base::Optional<Offset> getFieldOffsetByName(base::StrID field_name) const;
		[[nodiscard]]
		base::Optional<TypeCRef> getFieldTypeByOffset(Offset offset) const;
		[[nodiscard]]
		base::Optional<TypeCRef> getFieldTypeByOffsetRecursive(Offset offset) const;

		// inheritance
		[[nodiscard]]
		base::Optional<base::CRef<InheritanceMetadata>> getInheritanceMetadata() const;
		[[nodiscard]]
		base::Optional<TypeCRef> getSuperClass() const;
		[[nodiscard]]
		bool inheritsFrom(TypeCRef other) const;
		[[nodiscard]]
		bool isInstantiable() const;

		// variant
		[[nodiscard]]
		base::Optional<u64> getVariantCount() const;
		[[nodiscard]]
		base::Optional<TypeCRef> getNthVariantType(u64 variant_id) const;

		// function
		[[nodiscard]]
		base::Optional<u64> getParameterCount() const;
		[[nodiscard]]
		base::Optional<base::CRef<std::vector<TypeCRef>>> getParameters() const;
		[[nodiscard]]
		base::Optional<u64> getParametersSize() const;
		[[nodiscard]]
		base::Optional<TypeCRef> getNthParameterType(u64 parameter_id) const;
		[[nodiscard]]
		base::Optional<TypeCRef> getResultType() const;

		friend class TypeMetadata;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(Type, size);  // TODO: add better output of type
	};
}

JSON_REGISTER_TYPE_WITH_NAME(vm::Type, "Type");
