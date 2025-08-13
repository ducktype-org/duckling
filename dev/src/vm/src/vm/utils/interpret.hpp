#pragma once

#include <base/ints.hpp>

#include <cctype>
#include <cstring>
#include <new>
#include <print>
#include <type_traits>

namespace vm {
	/**
	 * @brief Safely reads an object of type T from a raw byte buffer.
	 *
	 * @note This function performs a bitwise copy from the buffer into a new
	 * object of type T. The `memcpy` operation is optimized by compilers to a single machine
	 * instruction for trivially copyable types.
	 * @note Type T must be trivially copyable.
	 *
	 * @tparam T The target type to construct. Must be trivially copyable.
	 * @param ptr A pointer to the beginning of the source byte buffer.
	 * @param offset An optional offset in bytes from the start of the buffer.
	 * @return A new object of type T, constructed from the bytes in the buffer.
	 */
	template<typename T>
	[[nodiscard]] inline T safeReadBytes(const byte* ptr, usize offset = 0)
		requires std::is_trivially_copyable_v<T> {
		alignas(T) byte buffer[sizeof(T)];
		std::memcpy(buffer, ptr + offset, sizeof(T));
		return *std::launder(reinterpret_cast<T*>(buffer));
		// return value;
	}

	/**
	 * @brief Safely writes the byte representation of an object to a buffer.
	 * @note This function performs a bitwise copy from the buffer into a new
	 * object of type T. It is complies with strict aliasing rules and memory alignment.
	 * The `memcpy` operation is optimized by compilers to a single machine instruction for
	 * trivially copyable types.
	 *
	 * @tparam T The type of the object to write. Must be trivially copyable.
	 * @param dest A pointer to the beginning of the destination byte buffer.
	 * @param value The object to write.
	 * @param offset An optional offset in bytes from the start of the buffer.
	 */
	template<typename T>
	inline void safeWriteBytes(byte* dest, const T& value, usize offset = 0)
		requires(std::is_trivially_copyable_v<T>) {
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
	[[nodiscard]] inline T safeReadBytes(const U& source_object) requires(
		std::is_trivially_copyable_v<T> && std::is_trivially_copyable_v<U> && sizeof(T) <= sizeof(U)
	) {
		return safeReadBytes<T>(reinterpret_cast<const byte*>(&source_object));
	}

	inline void hexdump(void* ptr, int buflen) {
		auto* buf = (unsigned char*) ptr;
		int   i = 0, j = 0;
		for (i = 0; i < buflen; i += 16) {
			std::print("{:06x}: ", i);
			for (j = 0; j < 16; j++)
				if (i + j < buflen)
					std::print("{:02x} ", buf[i + j]);
				else
					std::print("   ");
			std::print(" ");
			for (j = 0; j < 16; j++)
				if (i + j < buflen) std::print("{:c}", isprint(buf[i + j]) ? buf[i + j] : '.');
			std::println("");
		}
	}
}
