#pragma once

#include <base/extend_cpp/stringifyable_enum.hpp>
#include <base/pointers/box.hpp>
#include <base/types/bits_and_bytes.hpp>
#include <base/types/ints.hpp>

#include <string_id/string_id.hpp>

#include <vm/bytecode/element_base.hpp>

#include <array>
#include <cstring>
#include <utility>
#include <vector>

MAKE_STRINGIFYABLE_ENUM(vm::code, std::uint8_t, ConstValueType,
	Immediate, Class, FixedSizeTable
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
	 * @brief An immediate constant value stored as raw bytes.
	 * The number of meaningful bytes is tracked separately from the storage.
	 * Values of all primitive types (i8, i16, i32, i64, u8, ..., f32, f64, etc.)
	 * are bit-cast into the content array. The `size` field indicates how many
	 * bytes are actually used, which must match the declared type's byte size.
	 */
	class ConstantImmediate final: public ConstantBase {
	public:
		alignas(8) std::array<byte, 8> content{};
		Bytes size{ 0 };

		[[nodiscard]] Box<ConstantBase> clone() const override;

		[[nodiscard]] ConstValueType type() const override { return ConstValueType::Immediate; }

		ConstantImmediate() = default;

		/**
		 * @brief Constructs an immediate from raw bytes.
		 * @param content The raw bytes (only the first `size` bytes are meaningful).
		 * @param size Number of meaningful bytes (1-8).
		 */
		ConstantImmediate(std::array<byte, 8> content, Bytes size):
			  content(content),
			  size(size) {}

		/**
		 * @brief Constructs an immediate from a numeric value via bit_cast.
		 * @tparam T The type of the value (must be trivially copyable, sizeof(T) <= 8).
		 * @param value The numeric value.
		 */
		template<typename T>
		requires(sizeof(T) <= 8 && std::is_trivially_copyable_v<T>)
		static ConstantImmediate fromValue(T value) {
			ConstantImmediate result;
			result.size = Bytes(sizeof(T));
			std::memcpy(result.content.data(), &value, sizeof(T));
			return result;
		}

		static ConstantImmediate fromU64AndSize(u64 value, Bytes size) {
			CORE_ASSERT(size.asInt() <= 8, "Size must be <= 8 bytes");
			ConstantImmediate result;
			result.size = size;
			std::memcpy(result.content.data(), &value, size.asInt());
			return result;
		}

		void acceptVisitor(ConstVisitor&) const final;
	};

	class ConstantClass final: public ConstantBase {
	public:
		std::vector<std::pair<base::StrID, Box<ConstantBase>>> fields;

		ConstantClass() = default;

		[[nodiscard]] Box<ConstantBase> clone() const override;

		[[nodiscard]] ConstValueType type() const override { return ConstValueType::Class; }

		void acceptVisitor(ConstVisitor&) const final;
	};

	class ConstantFixedSizeTable final: public ConstantBase {
	public:
		std::vector<Box<ConstantBase>> elements;

		ConstantFixedSizeTable() = default;


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
	 * (ConstantClass, ConstantFixedSizeTable) since they
	 * contain ConstantValue within themselves.
	 *
	 * Grammar:
	 *   <expr> = class { field_name: <expr>, ... }
	 *   <expr> = fixed_size_table [ <expr>, <expr>, ... ]
	 *   <expr> = <immediate value>  (fallback: numeric literal)
	 */
	struct ConstantValue final: ElementBase {
		Box<ConstantBase> data;

		ConstantValue(Box<ConstantBase> d): data(std::move(d)) {}

		static ConstantValue fromImmediate(ConstantImmediate immediate) {
			return { makeBox<ConstantImmediate>(std::move(immediate)) };
		}

		static ConstantValue fromU64AndSize(u64 value, Bytes size) {
			return fromImmediate(ConstantImmediate::fromU64AndSize(value, size));
		}

		template<std::derived_from<ConstantBase> DataTypeElement>
		static ConstantValue fromData(Box<DataTypeElement>&& element) {
			return { std::move(element) };
		}

		ConstantValue(ConstantValue&&)            = default;
		ConstantValue& operator=(ConstantValue&&) = default;

		ConstantValue(const ConstantValue&);
		ConstantValue& operator=(const ConstantValue&);
	};
}  // namespace vm::code
