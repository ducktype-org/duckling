#pragma once

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/comptime/type_traits.hpp>
#include <base/extend_cpp/strongly_typed_id.hpp>
#include <base/types/bits_and_bytes.hpp>

#include <string_id/string_id.hpp>

#include "vm/utils/stable_obj_id_name_map.hpp"
#include <vm/bytecode/type_of_data.hpp>

#include <unordered_set>
#include <variant>

namespace vm::code::type {
	// @TODO: Can this become a Ref<Type>?
	// @TODO: #1306 Maybe it can become Ref<Type>?
	using TypeID = usize;

	namespace concrete {
		struct Primitive {
			usize size;
		};

		struct Pointer {
			TypeID inner;

			Pointer(TypeID inner): inner(inner) {}
		};

		struct FixedSizeTable {
			TypeID inner;
			usize  table_size;
		};

		struct DynamicTable {
			TypeID inner;
		};

		/**
		 * @brief A field inside a Structure type.
		 */
		struct Field {
			STRONG_TYPEDEF_ID_DIRECT_CREATION(ID);
			Bytes  offset;
			TypeID type;
		};

		struct InheritanceMetadata {
			// All the types this type directly or indirectly inherits from.
			std::unordered_set<TypeID> super_types;

			// In a class-like way. Only one superclass allowed
			base::Optional<TypeID> extends;

			// In a interface-like way. Multiple interfaces allowed
			std::unordered_set<TypeID> implements;

			bool is_abstract = false;

			enum Kind { Class, Interface } kind;
		};

		/**
		 * @brief Struct/Class (data) type representation.
		 * If declared as a class contains a vtable field at the front of field vector.
		 */
		struct Structure {
			ObjIdNameMap<Field, Field::ID>      fields;
			base::Optional<InheritanceMetadata> inheritance_metadata;
		};

		struct Variant {
			usize               type_tag_size_bytes;
			std::vector<TypeID> alternatives;
		};

		struct Function {
			std::vector<TypeID> parameters;
			TypeID              result;
		};

		struct Opaque {
			usize size;
		};
	}

#define CONCRETE_TYPE_LIST                                                                    \
	concrete::Primitive, concrete::Pointer, concrete::FixedSizeTable, concrete::DynamicTable, \
		concrete::Structure, concrete::Variant, concrete::Function, concrete::Opaque

	template<class T>
	concept ConcreteType = base::IsOneOf<T, CONCRETE_TYPE_LIST>;

	class Type {
		enum class State { Declared, Defined, Finalizing, Finalized } state = State::Declared;

	public:
		static Type declareType(base::StrID name, TypeID id);

		void definePrimitive(usize size) {
			CORE_ASSERT(state == State::Declared, "Bad type define");
			state = State::Defined;

			kind = concrete::Primitive{ size };
			if (name == "void") am_i_instantiable = false;
		}

		void definePointer(TypeID inner);
		void defineFixedSizeTable(TypeID inner, usize table_size);
		void defineDynamicTable(TypeID inner);
		void defineData(
			const std::vector<std::pair<base::StrID, TypeID>>& fields_definitions,
			base::Optional<concrete::InheritanceMetadata>      inheritance_metadata
		);
		void defineVariant(const std::vector<TypeID>& variant_types);
		void defineFunction(const std::vector<TypeID>& parameters, TypeID result);
		void defineOpaque(usize size);

		[[nodiscard]] base::StrID getName() const { return name; }

		template<ConcreteType T>
		[[nodiscard]]
		const T& get() const {
			return std::get<T>(kind);
		}

		template<ConcreteType T>
		[[nodiscard]]
		const base::Optional<Ref<T>> maybeGet() const {
			if (!is<T>()) return {};
			return &std::get<T>(kind);
		}

		template<ConcreteType T>
		[[nodiscard]]
		bool is() const {
			return std::holds_alternative<T>(kind);
		}

		bool operator==(const Type& other) const { return other.id == id; }

		// [[nodiscard]]
		// Bytes getSize() const {
		// 	CORE_ASSERT(size != TypeSize(-1), "getSize called before type finalization");
		// 	return size;
		// }

		// /**
		//  * Get inner type of pointer, fixed size or dynamic table
		//  * @return Some(inner type) for pointer, fixed size or dynamic table. none otherwise
		//  */
		// base::Optional<TypeCRef> getInnerType() const;

		// [[nodiscard]]
		// bool isTriviallyCopyable() const;

		// // data
		// [[nodiscard]]
		// base::Optional<Bytes> getFieldOffsetByName(base::StrID field_name) const;
		// [[nodiscard]]
		// base::Optional<base::CRef<std::vector<kind::DataField>>> getFields() const;

		// // variant
		// base::Optional<usize>                 getTypeTagSizeBytes() const;
		// base::Optional<std::vector<TypeCRef>> getVariantAlternatives() const;

		// // inheritance
		// [[nodiscard]]
		// base::Optional<base::CRef<InheritanceMetadata>> getInheritanceMetadata() const;
		// [[nodiscard]]
		// base::Optional<TypeCRef> getSuperClass() const;
		// [[nodiscard]]
		// bool inheritsFrom(TypeCRef other) const;
		// [[nodiscard]]
		// bool isInstantiable() const;

		// // function
		// [[nodiscard]]
		// base::Optional<u64> getParameterCount() const;
		// [[nodiscard]]
		// base::Optional<u64> getParametersSize() const;
		// [[nodiscard]]
		// base::Optional<TypeCRef> getNthParameterType(u64 parameter_id) const;
		// [[nodiscard]]
		// base::Optional<TypeCRef> getResultType() const;

	private:
		bool am_i_instantiable = true;

		Type(base::StrID name, TypeID id): name(name), id(id) {}

		base::StrID name;
		TypeID      id;

		std::variant<std::monostate, CONCRETE_TYPE_LIST> kind;
	};

}
