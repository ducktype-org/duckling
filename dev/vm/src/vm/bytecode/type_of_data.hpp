#pragma once

#include <base/string_id.hpp>
#include <base/variant.hpp>

#include <vm/bytecode/element_base.hpp>

#include <ostream>

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

		DataType() = default;

		DataType(base::StrID name, std::vector<Field> fields):
			  name(name),
			  fields(std::move(fields)) {}

		base::StrID        name;
		std::vector<Field> fields;

		bool operator==(const DataType& other) const {
			return name == other.name && fields == other.fields;
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
	 * @brief Storage for any type of bytecode data.
	 */
	using TypeOfData = std::variant<
		PrimitiveType,
		PointerType,
		StaticTableType,
		DynamicTableType,
		DataType,
		VariantType,
		FunctionType>;

	constexpr base::StrID typeName(const TypeOfData& type) {
		return VISIT(type, tp, return tp.name);
	}
}
