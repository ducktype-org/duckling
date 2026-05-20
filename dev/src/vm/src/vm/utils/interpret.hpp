#pragma once

#include <base/except/exceptions.hpp>
#include <base/types/ints.hpp>

#include <array>
#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>

namespace vm {

	template<std::size_t Alignment, typename T>
	constexpr bool is_aligned(const T* ptr) noexcept {
		return (reinterpret_cast<std::uintptr_t>(ptr) & (Alignment - 1)) == 0;
	}

	constexpr usize align_up(usize value, usize alignment) noexcept {
		return alignment == 0 ? value : ((value + alignment - 1) / alignment) * alignment;
	}

	template<typename T>
	constexpr bool is_naturally_aligned(const T* ptr) noexcept {
		return is_aligned<alignof(T)>(ptr);
	}

	/**
	 * @brief Safely reads an object of type T from a raw byte buffer.
	 *
	 * @note This function performs a bitwise copy from the buffer into a new
	 * object of type T. The `memcpy` operation is optimized by compilers to a single machine
	 * instruction for trivially copyable types.
	 * @note Type T must be trivially copyable.
	 * @warning Well defined if compiled with C++20 or newer, otherwise UB.
	 *
	 * @tparam T The target type to construct. Must be trivially copyable.
	 * @param ptr A pointer to the beginning of the source byte buffer.
	 * @param offset An optional offset in bytes from the start of the buffer.
	 * @return A new object of type T, constructed from the bytes in the buffer.
	 */
	template<typename T>
	[[nodiscard]] T safeReadPointerBytes(const byte* ptr, usize offset = 0)
		requires std::is_trivially_copyable_v<T> {
		CORE_ASSERT(
			is_naturally_aligned<T>(reinterpret_cast<const T*>(ptr + offset)),
			"Unaligned access in safeReadPointerBytes"
		);
		// @note: We create a byte array aligned as type T to prevent alignment-related UBs.
		// Doing it like below makes it impossible to "reinterpret" values of type T with private
		// constructors, thus the workaround:
		// T value;
		// std::memcpy(&value, ptr + offset, sizeof(T));

		alignas(T) std::array<byte, sizeof(T)> buffer;
		std::memcpy(buffer.data(), ptr + offset, sizeof(T));

		// @note: Reinterpret the buffer as a pointer to T and dereference it.
		// std::launder is necessary to tell the compiler that the object's lifetime
		// has begun at this memory location, and it can safely access the new value.
		return *std::launder(reinterpret_cast<T*>(buffer.data()));
	}

	/**
	 * @brief Safely writes the byte representation of an object to a buffer.
	 * @note This function performs a bitwise copy from the buffer into a new
	 * object of type T. It is compiled with strict aliasing rules and memory alignment.
	 * The `memcpy` operation is optimized by compilers to a single machine instruction for
	 * trivially copyable types.
	 *
	 * @tparam T The type of the object to write. Must be trivially copyable.
	 * @param dest A pointer to the beginning of the destination byte buffer.
	 * @param value The object to write.
	 * @param offset An optional offset in bytes from the start of the buffer.
	 */
	template<typename T>
	void safeWriteBytes(byte* dest, const T& value, usize offset = 0)
		requires(std::is_trivially_copyable_v<T>) {
		CORE_ASSERT(
			is_naturally_aligned<T>(reinterpret_cast<const T*>(dest + offset)),
			"Unaligned access in safeWriteBytes"
		);
		std::memcpy(dest + offset, &value, sizeof(T));
	}

	/**
	 * @brief Safely reinterprets a source object's bytes as a new object of type T.
	 *
	 * @note This function performs a bitwise copy from the buffer into a new
	 * object of type T. The `memcpy` operation is optimized by compilers to a single machine
	 * instruction for trivially copyable types.
	 * @note Both T and U must be trivially copyable.
	 * @note `sizeof(T)` must be less than or equal to `sizeof(U)`.
	 *
	 * @tparam T The target type to construct.
	 * @tparam U The type of the source object.
	 * @param source_object The object whose bytes will be read.
	 * @return A new object of type T, constructed from the bytes of `source_object`.
	 *
	 */
	template<typename T, typename U>
	[[nodiscard]] T safeReadObjectBytes(const U& source_object) requires(
		std::is_trivially_copyable_v<T> && std::is_trivially_copyable_v<U> && sizeof(T) <= sizeof(U)
	) {
		return safeReadPointerBytes<T>(reinterpret_cast<const byte*>(&source_object));
	}
}
