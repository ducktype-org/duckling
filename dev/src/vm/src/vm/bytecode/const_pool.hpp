#pragma once

#include <base/pointers/box.hpp>
#include <base/types/ints.hpp>

#include <string_id/string_id.hpp>

#include <utility>
#include <variant>
#include <vector>

namespace vm::code {
	/**
	 * @brief A primitive constant value stored as raw u64 bits.
	 * All numerical types (i8, i16, i32, i64, u8, ..., f32, f64, etc.)
	 * are bit-cast to u64 for storage.
	 */
	struct ConstantU64 {
		u64 value{ 0 };

		bool operator==(const ConstantU64& other) const { return value == other.value; }
	};

	struct ConstantClass;
	struct ConstantFixedSizeTable;

	/**
	 * @brief Storage for any kind of constant value expression.
	 *
	 * Uses unique_ptr indirection for mutually-recursive types
	 * (ConstantStructure, ConstantArray, ConstantVariant) since they
	 * contain ConstantValue within themselves.
	 *
	 * Grammar:
	 *   <expr> = structure { field_name: <expr>, ... }
	 *   <expr> = fixed_size_table [ <expr>, <expr>, ... ]
	 *   <expr> = u64 value  (fallback: numeric literal)
	 */
	struct ConstantValue {
		using DataType = std::variant<ConstantU64, ConstantClass, ConstantFixedSizeTable>;

		Box<DataType> data;

		ConstantValue(Box<DataType> d): data(std::move(d)) {}

		template<typename... Args>
		static ConstantValue from(Args&&... args) {
			return { makeBox<DataType>(std::forward<Args>(args)...) };
		}

		static ConstantValue fromU64(u64 value) { return { makeBox<ConstantU64>(value) }; }

		template<typename DataTypeElement>
		static ConstantValue fromData(Box<DataTypeElement>&& element) {
			return { std::move(element) };
		}

		ConstantValue(ConstantValue&&)            = default;
		ConstantValue& operator=(ConstantValue&&) = default;

		bool operator==(const ConstantValue& other) const;
	};

	struct ConstantClass {
		std::vector<std::pair<base::StrID, ConstantValue>> fields;

		bool operator==(const ConstantClass& other) const = default;
	};

	struct ConstantFixedSizeTable {
		std::vector<ConstantValue> elements;

		bool operator==(const ConstantFixedSizeTable& other) const = default;
	};
}  // namespace vm::code
