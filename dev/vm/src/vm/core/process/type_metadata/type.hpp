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

		base::StrID name;
		TypeSize    size      = TypeSize(-1);
		Kind        kind_type = Kind::None;
		TypeID      id;

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
		static Type declareType(base::StrID name);

		// Type definition:
		void definePrimitive(TypeSize size);
		void definePointer(TypeCRef inner);
		void defineStaticTable(TypeRef inner, u64 table_size);
		void defineDynamicTable(TypeRef inner);
		void defineData(
			const std::vector<std::pair<base::StrID, TypeRef>>& fields_definitions,
			const base::Optional<VTable>&                       vtable
		);
		void defineVariant(const std::vector<TypeRef>& variants_definitions);
		void defineFunction(std::vector<TypeCRef> parameters, TypeCRef result);

		// Type finalization:
		void finalize();

		// Type query:
		[[nodiscard]]
		inline TypeID getID() const {
			return id;
		}

		[[nodiscard]]
		inline base::StrID getName() const {
			return name;
		}

		[[nodiscard]]
		inline TypeSize getSize() const {
			CORE_ASSERT(size != TypeSize(-1), "getSize called before type finalization");
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
		[[nodiscard]]
		base::Optional<usize> getFieldCount() const;
		[[nodiscard]]
		base::Optional<TypeCRef> getFieldType(kind::Data::FieldID fieldID) const;
		[[nodiscard]]
		base::Optional<Offset> getFieldOffset(kind::Data::FieldID fieldID) const;
		[[nodiscard]]
		base::Optional<TypeCRef> getFieldTypeByOffset(Offset offset) const;
		[[nodiscard]]
		base::Optional<TypeCRef> getFieldTypeByOffsetRecursive(Offset offset) const;

		// inheritance
		[[nodiscard]]
		base::Optional<const VTable&> getVTable() const;
		[[nodiscard]]
		bool inheritsFrom(TypeCRef other) const;
		[[nodiscard]]
		bool isInstantiable() const;
		[[nodiscard]]
		bool isClass() const;
		[[nodiscard]]
		bool isInterface() const;
		[[nodiscard]]
		bool isPlain() const;


		// variant
		[[nodiscard]]
		base::Optional<u64> getVariantCount() const;
		[[nodiscard]]
		base::Optional<TypeCRef> getNthVariantType(u64 variantID) const;

		// function
		[[nodiscard]]
		base::Optional<u64> getParameterCount() const;
		[[nodiscard]]
		base::Optional<TypeCRef> getNthParameterType(u64 parameterID) const;
		[[nodiscard]]
		base::Optional<TypeCRef> getResultType() const;

		friend class TypeMetadata;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(Type, size);  // TODO: add better output of type
	};
}

JSON_REGISTER_TYPE_WITH_NAME(vm::Type, "Type");
