#pragma once

#include <base/types/ints.hpp>

#include <array>
#include <cstring>
#include <memory>
#include <new>
#include <type_traits>

namespace vm {

	/**
	 * @brief Checks if byte is sufficiently aligned for type T.
	 * @note replace with std::is_sufficiently_aligned, available since C++26
	 */
	template<typename T>
	constexpr bool isAligned(const byte* ptr) noexcept {
		std::size_t space = sizeof(T);
		// This function does not modify memory to with ptr points. It is legal to cast away const.
		void* ptr_copy = const_cast<void*>(reinterpret_cast<const void*>(ptr));  // NOLINT
		return std::align(alignof(T), sizeof(T), ptr_copy, space)
		    == reinterpret_cast<const void*>(ptr);
	}

	/**
	 * @brief Safely reads an object of type T from a raw byte buffer.
	 *
	 * @note Assumes object of type T exists at location ptr+offset. Interpreted program passed
	 * validation so before every safeReadPointerBytes there is safeWriteBytes which uses placement
	 * new and creates actual object there.
	 *
	 * @tparam T The target type to construct. Must be trivially copyable.
	 * @param ptr A pointer to the beginning of the source byte buffer.
	 * @param offset An optional offset in bytes from the start of the buffer.
	 * @return A new object of type T, constructed from the bytes in the buffer.
	 */
	template<typename T>
	[[nodiscard]] T safeReadPointerBytes(const byte* ptr, usize offset = 0)
		requires std::is_trivially_copyable_v<T> {
		CORE_ASSERT(isAligned<T>(ptr + offset), "Unaligned access in safeReadPointerBytes");
		// After reinterpret cast the pointer points to the memory on which object of type T
		// was created using placement new. That means dereferenced value is of type T.
		// T is type accesible to T - new pointer can be dereferenced.
		// https://cppreference.com/cpp/language/reinterpret_cast point 5.
	//	return *std::launder(reinterpret_cast<const T*>(ptr + offset));
		return *reinterpret_cast<const T*>(ptr + offset);
	
	}

	/**
	 * @brief Safely writes the byte representation of an object to a buffer.
	 * @note Creates object of type T at desired place using placement new.
	 * Actual object is created, its lifetime starts. We can treat this memory
	 * as if object of type T is stored there, without violating strict aliasing.
	 *
	 * @tparam T The type of the object to write. Must be trivially copyable.
	 * @param dest A pointer to the beginning of the destination byte buffer.
	 * @param value The object to write.
	 * @param offset An optional offset in bytes from the start of the buffer.
	 */
	template<typename T>
	void safeWriteBytes(byte* dest, const T& value, usize offset = 0)
		requires(std::is_trivially_copyable_v<T>) {
		CORE_ASSERT(isAligned<T>(dest + offset), "Unaligned access in safeWriteBytes");

		new (reinterpret_cast<void*>(dest + offset)) T(value);
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
