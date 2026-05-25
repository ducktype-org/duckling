#pragma once

#include "kinds.hpp"

#include <base/collections/optional.hpp>
#include <base/types/bits_and_bytes.hpp>

#include <string_id/string_id.hpp>

#include <vm/core/safe/memory/pointer.hpp>

#include <json/json.hpp>

#include <variant>

namespace vm {
	class TypeMetadata;

	class Type final {
	public:
		constexpr static TypeSize POINTER_SIZE = Bytes(sizeof(Pointer));


		/**
		 * @note Objects stored on stack are
		 * primitives (u64 has max aligment out of those)
		 * Pointer
		 * Opaque which are treated as bytes (aligment 1)
		 * Data and Fixed size table which has aligment of the most alinged field/inner type
		 * Variant which has aligment of the most alinged alternative or type tag size (which is
		 * smaller than u64)
		 */
		constexpr static size_t MAX_ALIGNMENT = std::max(alignof(Pointer), alignof(u64));

		/**
		 * @brief Aligns value up, to the Type::MAX_ALIGNMENT.
		 * @note Specific version of function for MAX_ALIGNMENT, because it is used in multiple
		 * places, MAX_ALIGNMENT is constexpr, so we can get more optimized function.
		 */
		constexpr static usize fullyAlignUp(usize value) {
			return alignUp(value, MAX_ALIGNMENT);
		}

		/**
		 * @brief Aligns value up to the alignment. Alignment must be a power of 2.
		 */
		constexpr static usize alignUp(usize value, size_t alignment) {
			return (value + alignment - 1) & ~(alignment - 1);
		}

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

		base::StrID            name;
		TypeSize               size = TypeSize(-1);
		base::Optional<size_t> stack_alignment
			= std::nullopt;  // Only types stored on stack need this.
		Kind   kind_type = Kind::None;
		TypeID id{};
		bool   am_i_instantiable = true;

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
		void definePrimitive(TypeSize size);
		void definePointer(TypeCRef inner);
		void defineFixedSizeTable(TypeRef inner, u64 table_size);
		void defineDynamicTable(TypeRef inner);
		void defineData(
			const std::vector<std::pair<base::StrID, TypeRef>>& fields_definitions,
			base::Optional<InheritanceMetadata>                 inheritance_metadata
		);
		void defineVariant(Bytes type_tag_size, const std::vector<TypeRef>& variants_definitions);
		void defineFunction(std::vector<TypeCRef> parameters, std::vector<TypeCRef> result);
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

		[[nodiscard]]
		size_t getStackAlignment() const {
			match_optional(stack_alignment) {
				opt_some(alignment) { return alignment; }
				opt_none {
					CORE_PANIC(
						"getStackAlignment called for type that is not stored on stack or before "
						"finalization"
					);
				}
			}
			CORE_UNREACHABLE();
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
		base::Optional<CRef<std::vector<kind::FieldDesc>>> getFields() const;

		// variant
		base::Optional<Bytes> getTypeTagSizeBytes() const;
		/// @note that includes type tag size + padding.
		base::Optional<Bytes>                 getVariantPayloadOffsetBytes() const;
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
