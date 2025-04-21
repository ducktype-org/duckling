#pragma once

#include <base/string_id.hpp>
#include <base/variant.hpp>

#include <vm/bytecode/element_base.hpp>

namespace vm::code {
	/**
	 * @brief Represents a very simple primitive, like 8-byte integer, 4-byte float, etc.
	 */
	struct PrimitiveType final: ElementBase {
		PrimitiveType() = default;

		PrimitiveType(const base::StrID name, const usize size): name(name), size(size) {}

		base::StrID name;
		usize       size{};

		bool operator==(const PrimitiveType& other) const {
			return name == other.name && size == other.size;
		}
	};

	/**
	 * @brief Represents a pointer.
	 */
	struct PointerType final: ElementBase {
		PointerType() = default;

		PointerType(base::StrID name, base::StrID inner): name(name), inner(inner) {}

		base::StrID name;
		base::StrID inner;

		bool operator==(const PointerType& other) const {
			return name == other.name && inner == other.inner;
		}
	};

	/**
	 * @brief Represents a fixed-size array of elements of the same type.
	 */
	struct StaticTableType final: ElementBase {
		StaticTableType() = default;

		StaticTableType(base::StrID name, base::StrID inner, usize table_size):
			  name(name),
			  inner(inner),
			  table_size(table_size) {}

		base::StrID name;
		base::StrID inner;
		usize       table_size{};

		bool operator==(const StaticTableType& other) const {
			return name == other.name && inner == other.inner && table_size == other.table_size;
		}
	};

	/**
	 * @brief Represents a dynamic array of elements of the same type.
	 */
	struct DynamicTableType final: ElementBase {
		DynamicTableType() = default;

		DynamicTableType(base::StrID name, base::StrID inner): name(name), inner(inner) {}

		base::StrID name;
		base::StrID inner;

		bool operator==(const DynamicTableType& other) const {
			return name == other.name && inner == other.inner;
		}
	};

	/**
	 * @brief Represents a structure with arbitrary types of fields.
	 */
	struct DataType final: ElementBase {
		/**
		 * @brief Field is a building block of a datatype. It represents a storage
		 * for value of some type.
		 */
		struct Field final: ElementBase {
			Field() = default;

			Field(base::StrID name, base::StrID type): name(name), type(type) {}

			base::StrID name;
			base::StrID type;

			bool operator==(const Field& other) const {
				return name == other.name && type == other.type;
			}
		};

		/**
		 * @brief Represents the VTable classes and interfaces use for
		 * dynamic dispatch. Plain datatypes don't have one.
		 */
		struct VTable {
			struct Class {
				bool                        is_abstract;
				base::Optional<base::StrID> extends;

				bool operator==(const Class& other) const = default;
			};

			struct Interface {
				bool operator==(const Interface& other) const = default;
			};

			using Kind = std::variant<Interface, Class>;

			Kind                     kind;
			std::vector<base::StrID> implements;
			std::vector<Field>       virtual_methods;

			bool operator==(const VTable& other) const = default;
		};

		DataType() = default;

		DataType(base::StrID name, std::vector<Field> fields, base::Optional<VTable> vtable):
			  name(name),
			  fields(std::move(fields)),
			  vtable(std::move(vtable)) {}

		base::StrID            name;
		std::vector<Field>     fields;
		base::Optional<VTable> vtable;

		bool operator==(const DataType& other) const {
			return name == other.name && fields == other.fields && vtable == other.vtable;
		}
	};

	/**
	 * @brief Represents a variant of types.
	 * @note This is a partial feature, as there are no bytecode instructions regarding variants.
	 */
	struct VariantType final: ElementBase {
		VariantType() = default;

		VariantType(base::StrID name, std::vector<base::StrID> variant_alternatives):
			  name(name),
			  variant_alternatives(std::move(variant_alternatives)) {}

		base::StrID              name;
		std::vector<base::StrID> variant_alternatives;

		bool operator==(const VariantType& other) const {
			return name == other.name && variant_alternatives == other.variant_alternatives;
		}
	};

	/**
	 * @brief Represents a function pointer type.
	 * @note This is currently as a declaration of a function with corresponding name. This is
	 * likely to change.
	 */
	struct FunctionType final: ElementBase {
		FunctionType(base::StrID name, std::vector<base::StrID> parameters, base::StrID result):
			  name(name),
			  parameters(std::move(parameters)),
			  result(result) {}

		base::StrID              name;
		std::vector<base::StrID> parameters;
		base::StrID              result;

		bool operator==(const FunctionType& other) const {
			return name == other.name && parameters == other.parameters && result == other.result;
		}
	};

	/**
	 * @brief Represents a type that is neither primitive nor composite.
     * Values of this kind can offer some operations, but they are opaque
     * to the bytecode program. Used for VTables.
	 */
	struct OpaqueType final: ElementBase {
		OpaqueType() = default;

		OpaqueType(const base::StrID name, const usize size): name(name), size(size) {}

		base::StrID name;
		usize       size{};

		bool operator==(const OpaqueType& other) const {
			return name == other.name && size == other.size;
		}
	};

	/**
	 * @brief Storage for any type of bytecode data.
	 */
	using TypeOfData = std::variant<
		PrimitiveType,
		PointerType,
		StaticTableType,
		DynamicTableType,
		DataType,
		VariantType,
		FunctionType,
		OpaqueType>;

	constexpr base::StrID typeName(const TypeOfData& type) {
		return VISIT(type, tp, return tp.name);
	}
}
