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
			CPointer,
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
		bool        am_i_instantiable = true;

		/// Set by every `define*`, see `getShadowSize`.
		base::Optional<ShadowSize> shadow_size;

		std::variant<
			std::monostate,
			kind::Primitive,
			kind::Pointer,
			kind::CPointer,
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

		// Type definition. The validator (`valid_type::ValidType`) computes every shadow size,
		// the runtime only stores it, see `getShadowSize`.
		void definePrimitive(TypeSize size, ShadowSize shadow_size);
		void definePointer(TypeCRef inner, ShadowSize shadow_size);
		/**
		 * @brief Defines a C pointer: a raw 8-byte native address. An absent inner means an
		 * unknown pointee (C's `void*`).
		 */
		void defineCPointer(base::Optional<TypeCRef> inner, ShadowSize shadow_size);
		void defineFixedSizeTable(TypeRef inner, u64 table_size, ShadowSize shadow_size);
		void defineDynamicTable(TypeRef inner, ShadowSize shadow_size);
		/**
		 * @brief Defines a data type from a validator-computed layout. The validator
		 * (`valid_type::ValidType`) is the source of truth for field offsets (byte and shadow) and
		 * the total size; the runtime does not compute any layout itself.
		 */
		void defineData(
			const std::vector<std::tuple<base::StrID, TypeRef, Offset, ShadowOffset>>&
												fields_definitions,
			TypeSize                            data_size,
			base::Optional<InheritanceMetadata> inheritance_metadata,
			ShadowSize                          shadow_size
		);
		void defineVariant(
			Bytes                       type_tag_size,
			const std::vector<TypeRef>& variants_definitions,
			ShadowSize                  shadow_size
		);
		void defineFunction(
			std::vector<TypeCRef> parameters, std::vector<TypeCRef> result, ShadowSize shadow_size
		);
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

		/**
		 * @brief Number of shadow entries (Fast Track's per-location race-detection state) an
		 * object of this type occupies, see `valid_type::ValidType::getShadowSize`.
		 */
		[[nodiscard]]
		ShadowSize getShadowSize() const {
			return shadow_size.expect("getShadowSize called before type definition");
		}

		/**
		 * @brief Shadow entry index of the byte at `byte_offset` of an object of this type.
		 *
		 * Scalars (primitives, pointers, functions, opaques) occupy a single entry, so every byte
		 * maps to entry 0. Tables map through their element type. Data types map through their
		 * fields, found by a binary search on the field offsets; padding bytes belong to no field
		 * and must not be asked about.
		 *
		 * A variant's tag maps to entry 0. Its payload layout depends on the active alternative,
		 * so from the variant itself every payload byte maps to entry 1, the first payload entry:
		 * only an access that treats the whole payload as one value may map through the variant.
		 * An access to a field of the active alternative has to go through the nested shadow
		 * block of that alternative, which starts at entry 1 and is the only way to reach entries
		 * 2 and up; such an access is therefore ordered against a whole-payload access only where
		 * it touches entry 1.
		 *
		 * @note `byte_offset` has to be inside the object. A dynamic table has no size of its own,
		 * so there the offset is only checked against the element size.
		 */
		[[nodiscard]] ShadowOffset getShadowEntryIndex(u64 byte_offset) const;

		/**
		 * @brief `getShadowEntryIndex` that answers `none` for a padding byte of a data type
		 * instead of failing, for callers that walk every byte of an object.
		 */
		[[nodiscard]] base::Optional<ShadowOffset> shadowEntryIndexOrPadding(u64 byte_offset) const;

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
		base::Optional<TypeCRef> getFieldTypeByName(base::StrID field_name) const;
		[[nodiscard]]
		base::Optional<CRef<std::vector<kind::FieldDesc>>> getFields() const;

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
