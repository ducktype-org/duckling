#pragma once

#include <base/pointers/ref.hpp>
#include <base/types/bits_and_bytes.hpp>

#include <string_id/string_id.hpp>

#include <vm/core/safe/type_metadata/inheritance_metadata.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>
#include <vm/core/fast/program/ids.hpp>

namespace vm::fast {
	struct Type;
	using TypeCRef = base::CRef<Type>;

	using TypeCollection = StableObjIdNameMap<Type, TypeID>;

	namespace kind {
		struct Primitive final {
			// For primitive types, the size of the type in bytes.
			Bytes size;
		};

		struct VariantData final {
			// For variant types, the size of the type tag in bytes.
			Bytes                 type_tag_size;
			std::vector<TypeCRef> variant_alternatives;
		};

		struct Pointer final {
			TypeCRef pointed_type;
		};

		struct FixedSizeTable final {
			TypeCRef element_type;
			u64      element_count;
		};

		struct DynamicTable final {
			TypeCRef element_type;
		};

		struct InheritanceMetadata final {};

		struct Data final {
			std::vector<std::pair<base::StrID, TypeCRef>> fields_definitions;
			base::Optional<InheritanceMetadata>           inheritance_metadata;
		};

		struct Function final {
			std::vector<TypeCRef> parameters;
			std::vector<TypeCRef> result_types;
		};

		struct Opaque final {
			Bytes size;
		};
	}

	struct Type final {
		base::StrID name;
		TypeID      id;
		Bytes       size;
		std::variant<
			base::Monostate,
			kind::Primitive,
			kind::Pointer,
			kind::VariantData,
			kind::FixedSizeTable,
			kind::DynamicTable,
			kind::Data,
			kind::Function,
			kind::Opaque>
			kind = base::Monostate{};

		/****************/
		/* Constructors */
		/****************/
		static Type declareType(base::StrID name, TypeID id, TypeSize size);

		// Type definition:
		void definePrimitive(Bytes size);
		void definePointer(TypeCRef inner);
		void defineFixedSizeTable(TypeCRef inner, u64 table_size);
		void defineDynamicTable(TypeCRef inner);
		void defineData(
			const std::vector<std::pair<base::StrID, TypeCRef>>& fields_definitions,
			base::Optional<kind::InheritanceMetadata>            inheritance_metadata
		);
		void defineVariant(Bytes type_tag_size, const std::vector<TypeCRef>& variants_definitions);
		void defineFunction(std::vector<TypeCRef> parameters, std::vector<TypeCRef> result);
		void defineOpaque(Bytes size);

		[[nodiscard]]
		TypeID getID() const;

		[[nodiscard]]
		base::StrID getName() const;

		[[nodiscard]]
		TypeSize getSize() const;

		template<class T>
		base::Optional<CRef<T>> get() const {
			if (std::holds_alternative<T>(kind)) return &std::get<T>(kind);
			return {};
		}

		[[nodiscard]]
		auto getKindVariant() const {
			return kind;
		}
	};
}
