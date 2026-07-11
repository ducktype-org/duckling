#pragma once

#include "kinds.hpp"

#include <base/collections/optional.hpp>
#include <base/types/bits_and_bytes.hpp>

#include <string_id/string_id.hpp>

#include <vm/core/safe/memory/pointer.hpp>

#include <json/json.hpp>

#include <tuple>
#include <variant>

namespace vm {
	class TypeMetadata;

	class Type final {
	public:
		constexpr static TypeSize POINTER_SIZE = Bytes(sizeof(Pointer));

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
		TypeSize    size        = TypeSize(-1);
		ShadowSize  shadow_size = ShadowSize(-1);
		Kind        kind_type   = Kind::None;
		TypeID      id{};
		bool        am_i_instantiable = true;

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

		/**
		 * @brief Finds out if the type is instantiable knowing it is of kind::Data.
		 * @param data The stored kind.
		 */
		void isInstantiableImpl(kind::Data& data);

		/**
		 * @brief Finds out if the type is instantiable knowing it is of kind::Variant.
		 * @param data The stored kind.
		 */
		void isInstantiableImpl(kind::Variant& variant);

		/**
		 * @brief Finds out if the type is instantiable knowing it has inheritance.
		 * @param inheritanceMetadata InheritanceMetadata of the stored class/interface.
		 */
		void inheritsFromImpl(InheritanceMetadata& inheritance_metadata);

	public:
		// Type declaration:
		static Type declareType(base::StrID name);

		// Type definition:
		void definePrimitive(TypeSize size, ShadowSize shadow_size);
		void definePointer(TypeCRef inner, ShadowSize shadow_size);
		void defineFixedSizeTable(TypeRef inner, u64 table_size, ShadowSize shadow_size);
		void defineDynamicTable(TypeRef inner, ShadowSize shadow_size);
		/**
		 * @brief Defines a data type from a validator-computed layout. The validator
		 * (`valid_type::ValidType`) is the source of truth for field byte offsets, shadow offsets,
		 * and the total size; the runtime does not compute any layout itself.
		 */
		void defineData(
			const std::vector<std::tuple<base::StrID, TypeRef, Offset, ShadowOffset>>&
			                                     fields_definitions,
			TypeSize                             data_size,
			base::Optional<InheritanceMetadata>  inheritance_metadata,
			ShadowSize                           shadow_size
		);
		void defineVariant(Bytes type_tag_size, const std::vector<TypeRef>& variants_definitions, ShadowSize shadow_size);
		void defineFunction(std::vector<TypeCRef> parameters, std::vector<TypeCRef> result, ShadowSize shadow_size);
		void defineOpaque(TypeSize size, ShadowSize shadow_size);

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

		[[nodiscard]]
		ShadowSize getShadowSize() const {
			CORE_ASSERT(shadow_size != ShadowSize(-1), "getShadowSize called before type finalization");
			return shadow_size;
		}

		template<class T>
		base::Optional<CRef<T>> get() const {
			if (std::holds_alternative<T>(kind)) return &std::get<T>(kind);
			return {};
		}

		[[nodiscard]]
		Kind getKind() const {
			return kind_type;
		}

		[[nodiscard]]
		auto getKindVariant() const {
			return kind;
		}

		// @todo: Interface below may change

		// @TODO: move function below to kind:: structures without `option`
		// Forward here version with option

		/**
		 * Get inner type of pointer, fixed size or dynamic table
		 * @return some(inner type) for pointer, fixed size or dynamic table. none otherwise
		 */
		base::Optional<TypeCRef> getInnerType() const;

		bool isTriviallyCopyable() const;

		// data
		[[nodiscard]]
		base::Optional<Offset> getFieldOffsetByName(base::StrID field_name) const;
        [[nodiscard]]
        base::Optional<ShadowOffset> getFieldShadowOffsetByName(base::StrID field_name) const;
		[[nodiscard]]
		base::Optional<TypeCRef> getFieldTypeByName(base::StrID field_name) const;
		[[nodiscard]]
		base::Optional<CRef<std::vector<kind::FieldDesc>>> getFields() const;

		[[nodiscard]] u32 getShadowEntryIndex(u64 byte_offset) const;
		void setByteToShadow(std::vector<u32> b2s);

		// variant
		base::Optional<Bytes>                 getTypeTagSizeBytes() const;
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
		base::Optional<Bytes> getParametersSize() const;
		[[nodiscard]]
		base::Optional<TypeCRef> getNthParameterType(u64 parameter_id) const;
		[[nodiscard]]
		base::Optional<u64> getResultTypeCount() const;
		[[nodiscard]]
		base::Optional<Bytes> getResultTypeSize() const;
		[[nodiscard]]
		base::Optional<TypeCRef> getNthResultType(u64 parameter_id) const;

		friend class TypeMetadata;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(Type, size);  // TODO: add better output of type
	};
}

JSON_REGISTER_TYPE_WITH_NAME(vm::Type, "Type");
