#pragma once

#include "kinds.hpp"

#include "base/exceptions.hpp"
#include <base/optional.hpp>
#include <base/string_id.hpp>
#include <base/variant.hpp>

#include <vm/core/process/memory/pointer.hpp>

#include <json/json.hpp>

#include <variant>

namespace vm {
	class TypeMetadata;

	/// Size of type in bytes
	// @TODO: change to strongly typed int
	// @TODO check how to make STRONG_TYPEDEF_INT not interfere with NLOHMANN
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
			variant_match(kind) {
				variant_case_novalue(std::monostate) { return Kind::None; }
				variant_case_novalue(kind::Primitive) { return Kind::Primitive; }
				variant_case_novalue(kind::Pointer) { return Kind::Pointer; }
				variant_case_novalue(kind::FixedSizeTable) { return Kind::FixedSizeTable; }
				variant_case_novalue(kind::DynamicTable) { return Kind::DynamicTable; }
				variant_case_novalue(kind::Data) { return Kind::Data; }
				variant_case_novalue(kind::Variant) { return Kind::Variant; }
				variant_case_novalue(kind::Function) { return Kind::Function; }
				variant_case_novalue(kind::Opaque) { return Kind::Opaque; }
				variant_default { CORE_PANIC("This kind of Type is not implemented"); }
			};
			return Kind::None;
		}

		// @todo: Interface below may change

		// @TODO: move function below to kind:: structures without `option`
		// Forward here version with option

		/**
		 * Get inner type of pointer, fixed size or dynamic table
		 * @return some(inner type) for pointer, fixed size or dynamic table. none otherwise
		 */
		base::Optional<TypeCRef> getInnerType() const;

		// data
		[[nodiscard]]
		base::Optional<Offset> getFieldOffsetByName(base::StrID field_name) const;

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

		friend class TypeMetadata;

		// @TODO: add better output of type
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(Type, size);
	};
}

JSON_REGISTER_TYPE_WITH_NAME(vm::Type, "Type");
