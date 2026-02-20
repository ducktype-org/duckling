#pragma once

#include "base/types/bits_and_bytes.hpp"

namespace vm::code::type {
	/**
	 * @brief TypeSize consists of a number of non-pointer bytes and a number of pointer fields.
	 * This is useful for calculating the size of types, because we need to take into account
	 * different pointer sizes on different architectures.
	 * @note TypeSize is not directly comparable, because of the pointer size issue mentioned above.
	 * E.g. Structure of size 32 bytes (4 * i64) should be able to fit 3 pointers on regular 64-bit
	 * architecture, but once pointer size is increased to 16 bytes, the pointers do not fit
	 * anymore. This means type::TypeSize is uncomparable.
	 */
	struct TypeSize {
		constexpr TypeSize() = default;

		constexpr TypeSize(Bytes non_pointer_bytes, usize number_pointer_fields):
			  non_pointer_bytes(non_pointer_bytes),
			  number_pointer_fields(number_pointer_fields) {}

		[[nodiscard]] static constexpr TypeSize pointer() { return { Bytes(0), 1 }; }

		Bytes non_pointer_bytes;
		usize number_pointer_fields;

		TypeSize operator+(const TypeSize& other) const {
			return { non_pointer_bytes + other.non_pointer_bytes,
				     number_pointer_fields + other.number_pointer_fields };
		}

		TypeSize operator+=(const TypeSize& other) {
			non_pointer_bytes += other.non_pointer_bytes;
			number_pointer_fields += other.number_pointer_fields;
			return *this;
		}

		TypeSize operator*(usize multiplier) const {
			return { non_pointer_bytes * multiplier, number_pointer_fields * multiplier };
		}

		bool operator==(const TypeSize& other) const = default;

		[[nodiscard]] TypeSize fieldMax(const TypeSize& other) const {
			return { std::max(non_pointer_bytes, other.non_pointer_bytes),
				     std::max(number_pointer_fields, other.number_pointer_fields) };
		}

		[[nodiscard]] Bytes getTotalSize(Bytes pointer_size) const {
			return non_pointer_bytes + pointer_size * number_pointer_fields;
		}
	};
}
