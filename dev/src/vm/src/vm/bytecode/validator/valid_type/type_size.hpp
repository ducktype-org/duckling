#pragma once

#include <base/except/exceptions.hpp>
#include <base/types/bits_and_bytes.hpp>
#include <base/types/ints.hpp>

#include <hashing/hash.hpp>

namespace vm::code::valid_type {
	using ShadowSize  = u32;
	using PointerSize = u32;

	/**
	 * @brief TypeSize represents the size of a type in bytes. It takes into account the fact that
	 * pointer sizes can be different on different architectures. This is useful for calculating the
	 * size of structured types, like structures and variants, which can contain pointer fields.
	 * @note If TypeSize were to be a tuple of (Bytes non_pointer_bytes, usize
	 * number_of_pointer_fields) then it would be less useful, because of the pointer size issue:
	 * e.g. structure of size 32 bytes (4 * i64) should be able to fit 3 pointers on regular 64-bit
	 * architecture, but once pointer size is increased to 16 bytes, the pointers do not fit
	 * anymore. This means valid_type::TypeSize is uncomparable - we can't choose a "larger" size
	 * directly. This is why TypeSize has two separate fields for size when pointer size is 8 bytes
	 * and when pointer size is 16 bytes, so that at least we can perform `fieldMax` on two
	 * TypeSizes, which is useful for calculating the size of structures (mainly variant's data
	 * field size).
	 */
	class TypeSize {
	public:
		static constexpr base::Monostate HASHING_CAN_HASH_BY_REPRESENTATION = {};

		constexpr TypeSize() = default;

		constexpr TypeSize(Bytes non_pointer_bytes, usize number_pointer_fields):
			  size_when_ptr_is_8_bytes(non_pointer_bytes + Bytes(number_pointer_fields) * 8),
			  size_when_ptr_is_16_bytes(non_pointer_bytes + Bytes(number_pointer_fields) * 16) {}

		static constexpr TypeSize pointer() { return { Bytes(0), 1 }; }

		constexpr TypeSize(Bytes size_when_ptr_is_8_bytes, Bytes size_when_ptr_is_16_bytes):
			  size_when_ptr_is_8_bytes(size_when_ptr_is_8_bytes),
			  size_when_ptr_is_16_bytes(size_when_ptr_is_16_bytes) {}

		constexpr TypeSize operator+(const TypeSize& other) const {
			return { size_when_ptr_is_8_bytes + other.size_when_ptr_is_8_bytes,
				     size_when_ptr_is_16_bytes + other.size_when_ptr_is_16_bytes };
		}

		constexpr TypeSize operator+=(const TypeSize& other) {
			size_when_ptr_is_8_bytes += other.size_when_ptr_is_8_bytes;
			size_when_ptr_is_16_bytes += other.size_when_ptr_is_16_bytes;
			return *this;
		}

		constexpr TypeSize operator-(const TypeSize& other) const {
			CORE_ASSERT(
				size_when_ptr_is_8_bytes >= other.size_when_ptr_is_8_bytes
					&& size_when_ptr_is_16_bytes >= other.size_when_ptr_is_16_bytes,
				"Underflow in TypeSize subtraction"
			);
			return { size_when_ptr_is_8_bytes - other.size_when_ptr_is_8_bytes,
				     size_when_ptr_is_16_bytes - other.size_when_ptr_is_16_bytes };
		}

		constexpr TypeSize operator-=(const TypeSize& other) {
			*this = *this - other;
			return *this;
		}

		constexpr TypeSize operator*(usize multiplier) const {
			return { size_when_ptr_is_8_bytes * multiplier, size_when_ptr_is_16_bytes * multiplier };
		}

		constexpr bool operator==(const TypeSize& other) const = default;

		[[nodiscard]] constexpr TypeSize fieldMax(const TypeSize& other) const {
			return { std::max(size_when_ptr_is_8_bytes, other.size_when_ptr_is_8_bytes),
				     std::max(size_when_ptr_is_16_bytes, other.size_when_ptr_is_16_bytes) };
		}

		/**
		 * @brief Round each component up to the nearest multiple of the corresponding component of
		 * `alignment`. Used for computing aligned field offsets, where TypeSize doubles as a
		 * dual-width alignment value (alignment can differ between the 8- and 16-byte pointer
		 * modes, e.g. for pointer fields).
		 */
		[[nodiscard]] constexpr TypeSize alignedTo(const TypeSize& alignment) const {
			auto round_up = [](Bytes value, Bytes align) {
				CORE_ASSERT(usize(align) > 0, "Alignment must be positive");
				return Bytes((usize(value) + usize(align) - 1) / usize(align) * usize(align));
			};
			return { round_up(size_when_ptr_is_8_bytes, alignment.size_when_ptr_is_8_bytes),
				     round_up(size_when_ptr_is_16_bytes, alignment.size_when_ptr_is_16_bytes) };
		}

		[[nodiscard]] constexpr Bytes assumePointerSize(Bytes pointer_size) const {
			switch (usize(pointer_size)) {
			case 8:
				return size_when_ptr_is_8_bytes;
			case 16:
				return size_when_ptr_is_16_bytes;
			default:
				CORE_PANIC("Unsupported pointer size: ", pointer_size);
			}
		}

	private:
		Bytes size_when_ptr_is_8_bytes;
		Bytes size_when_ptr_is_16_bytes;
	};
}
