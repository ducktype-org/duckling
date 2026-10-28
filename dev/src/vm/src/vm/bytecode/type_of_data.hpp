// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/extend_cpp/variant_match.hpp>
#include <base/types/bits_and_bytes.hpp>

#include <string_id/string_id.hpp>

#include <vm/bytecode/element_base.hpp>
#include <vm/bytecode/type_expr.hpp>

#include <utility>

namespace vm::code {
	/**
	 * @brief Represents a very simple primitive, like 8-byte integer, 4-byte float, etc.
	 */
	struct PrimitiveType final: ElementBase {
		PrimitiveType() = default;

		PrimitiveType(const base::StrID name, const Bytes size): name(name), size(size) {}

		base::StrID name;
		Bytes       size{};

		bool operator==(const PrimitiveType& other) const {
			return name == other.name && size == other.size;
		}
	};

	/**
	 * @brief Represents a fixed-size array of elements of the same type.
	 */
	struct FixedSizeTableType final: ElementBase {
		FixedSizeTableType() = default;

		FixedSizeTableType(base::StrID name, TypeExpr inner, usize table_size):
			  name(name),
			  inner(inner),
			  table_size(table_size) {}

		base::StrID name;
		TypeExpr    inner;
		usize       table_size{};

		bool operator==(const FixedSizeTableType& other) const {
			return name == other.name && inner == other.inner && table_size == other.table_size;
		}
	};

	/**
	 * @brief Represents a dynamic array of elements of the same type.
	 */
	struct DynamicTableType final: ElementBase {
		DynamicTableType() = default;

		DynamicTableType(base::StrID name, TypeExpr inner): name(name), inner(inner) {}

		base::StrID name;
		TypeExpr    inner;

		bool operator==(const DynamicTableType& other) const {
			return name == other.name && inner == other.inner;
		}
	};

	/**
	 * @brief Field is a building block of a datatype. It represents a storage
	 * for value of some type.
	 */
	struct Field final: ElementBase {
		Field() = default;

		Field(base::StrID name, TypeExpr type): name(name), type(type) {}

		base::StrID name;
		TypeExpr    type;

		bool operator==(const Field& other) const {
			return name == other.name && type == other.type;
		}
	};

	/**
	 * @brief Binds a virtual method to the function implementing it.
	 * @note Kept apart from `Field` because the second member names a function, not a type.
	 */
	struct MethodImpl final: ElementBase {
		MethodImpl() = default;

		MethodImpl(base::StrID name, base::StrID function_name):
			  name(name),
			  function_name(function_name) {}

		base::StrID name;
		base::StrID function_name;

		bool operator==(const MethodImpl& other) const {
			return name == other.name && function_name == other.function_name;
		}
	};

	/**
	 * @brief Represents a structure with arbitrary types of fields.
	 */
	struct DataType final: ElementBase {
		DataType() = default;

		DataType(base::StrID name, std::vector<Field> fields):
			  name(name),
			  fields(std::move(fields)) {}

		base::StrID        name;
		std::vector<Field> fields;

		/// Whether fields are laid out without alignment padding (the `packed` modifier).
		bool packed = false;

		/// Optional expected byte size (assuming 8-byte pointers), pinned via `assert_size`.
		base::Optional<usize> assert_size;

		bool operator==(const DataType& other) const {
			return name == other.name && fields == other.fields && packed == other.packed
			    && assert_size == other.assert_size;
		}
	};

	/**
	 * @brief Represents a variant of types.
	 */
	struct VariantType final: ElementBase {
		VariantType() = default;

		VariantType(base::StrID name, std::vector<TypeExpr> variant_alternatives):
			  name(name),
			  variant_alternatives(std::move(variant_alternatives)) {}

		base::StrID           name;
		std::vector<TypeExpr> variant_alternatives;

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
		FunctionType(
			base::StrID name, std::vector<TypeExpr> parameters, std::vector<TypeExpr> result
		):
			  name(name),
			  parameters(std::move(parameters)),
			  result(std::move(result)) {}

		base::StrID           name;
		std::vector<TypeExpr> parameters;
		std::vector<TypeExpr> result;

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

		OpaqueType(const base::StrID name, const Bytes size): name(name), size(size) {}

		base::StrID name;
		Bytes       size{};

		bool operator==(const OpaqueType& other) const {
			return name == other.name && size == other.size;
		}
	};

	/**
	 * @brief Represents a class --- a data with virtual methods and inheritance.
	 */
	struct ClassType final: ElementBase {
		ClassType() = default;

		ClassType(
			base::StrID              name,
			std::vector<Field>       fields,
			bool                     is_abstract,
			base::Optional<TypeExpr> extends,
			std::vector<TypeExpr>    implements,
			std::vector<Field>       virtual_methods,
			std::vector<MethodImpl>  implementations
		):
			  name{ name },
			  fields{ std::move(fields) },
			  is_abstract{ is_abstract },
			  extends{ extends },
			  implements{ std::move(implements) },
			  virtual_methods{ std::move(virtual_methods) },
			  implementations{ std::move(implementations) } {}

		base::StrID              name;
		std::vector<Field>       fields;
		bool                     is_abstract{};
		base::Optional<TypeExpr> extends;
		std::vector<TypeExpr>    implements;
		std::vector<Field>       virtual_methods;
		std::vector<MethodImpl>  implementations;

		bool operator==(const ClassType& other) const {
			return name == other.name && fields == other.fields && is_abstract == other.is_abstract
			    && extends == other.extends && implements == other.implements
			    && virtual_methods == other.virtual_methods
			    && implementations == other.implementations;
		}
	};

	/**
	 * @brief Represents an interface. Interfaces do not hold any data.
	 */
	struct InterfaceType final: ElementBase {
		InterfaceType() = default;

		InterfaceType(
			base::StrID             name,
			std::vector<TypeExpr>   implements,
			std::vector<Field>      virtual_methods,
			std::vector<MethodImpl> implementations
		):
			  name{ name },
			  implements{ std::move(implements) },
			  virtual_methods{ std::move(virtual_methods) },
			  implementations{ std::move(implementations) } {}

		base::StrID             name;
		std::vector<TypeExpr>   implements;
		std::vector<Field>      virtual_methods;
		std::vector<MethodImpl> implementations;

		bool operator==(const InterfaceType& other) const {
			return name == other.name && implements == other.implements
			    && virtual_methods == other.virtual_methods
			    && implementations == other.implementations;
		}
	};

	/**
	 * @brief Storage for any type of bytecode data.
	 */
	using TypeOfData = std::variant<
		PrimitiveType,
		FixedSizeTableType,
		DynamicTableType,
		DataType,
		VariantType,
		FunctionType,
		OpaqueType,
		ClassType,
		InterfaceType>;

	constexpr base::StrID typeName(const TypeOfData& type) {
		return VISIT(type, tp, return tp.name);
	}

	template<typename T>
	constexpr base::Optional<T> getTypeKind(const TypeOfData& type) {
		return std::holds_alternative<T>(type) ? std::get<T>(type) : base::Optional<T>{};
	}
}
