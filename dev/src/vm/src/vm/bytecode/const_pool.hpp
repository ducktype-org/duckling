#pragma once

#include "base/extend_cpp/stringifyable_enum.hpp"
#include <base/pointers/box.hpp>
#include <base/types/ints.hpp>

#include <string_id/string_id.hpp>

#include <utility>
#include <vector>

MAKE_STRINGIFYABLE_ENUM(vm::code, std::uint8_t, ConstValueType, 
	U64, Class, FixedSizeTable
);

namespace vm::code {
	class ConstVisitor;

	class ConstantBase {
	public:
		virtual ~ConstantBase() = default;

		[[nodiscard]] virtual Box<ConstantBase> clone() const = 0;
		[[nodiscard]] virtual ConstValueType    type() const  = 0;

		virtual void acceptVisitor(ConstVisitor&) const = 0;
	};

	/**
	 * @brief A primitive constant value stored as raw u64 bits.
	 * All numerical types (i8, i16, i32, i64, u8, ..., f32, f64, etc.)
	 * are bit-cast to u64 for storage.
	 */
	class ConstantU64: public ConstantBase {
	public:
		u64 value{ 0 };

		[[nodiscard]] Box<ConstantBase> clone() const override;

		[[nodiscard]] ConstValueType type() const override { return ConstValueType::U64; }

		ConstantU64() = default;

		explicit ConstantU64(u64 value): value(value) {}

		void acceptVisitor(ConstVisitor&) const final;
	};

	class ConstantClass: public ConstantBase {
	public:
		std::vector<std::pair<base::StrID, Box<ConstantBase>>> fields;

		ConstantClass()                                   = default;

		[[nodiscard]] Box<ConstantBase> clone() const override;

		[[nodiscard]] ConstValueType type() const override { return ConstValueType::Class; }

		void acceptVisitor(ConstVisitor&) const final;
	};

	class ConstantFixedSizeTable: public ConstantBase {
	public:
		std::vector<Box<ConstantBase>> elements;

		ConstantFixedSizeTable()                                   = default;


		[[nodiscard]] Box<ConstantBase> clone() const override;

		[[nodiscard]] ConstValueType type() const override {
			return ConstValueType::FixedSizeTable;
		}

		void acceptVisitor(ConstVisitor&) const final;
	};

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
		MBox<ConstantBase> data;

		ConstantValue(MBox<ConstantBase> d): data(std::move(d)) {}

		ConstantValue() = default;

		static ConstantValue fromU64(u64 value) { return { makeBox<ConstantU64>(value) }; }

		template<typename DataTypeElement>
		static ConstantValue fromData(Box<DataTypeElement>&& element) {
			return { std::move(element) };
		}

		ConstantValue(ConstantValue&&)            = default;
		ConstantValue& operator=(ConstantValue&&) = default;

		ConstantValue(const ConstantValue&);
		ConstantValue& operator=(const ConstantValue&);
	};
}  // namespace vm::code
