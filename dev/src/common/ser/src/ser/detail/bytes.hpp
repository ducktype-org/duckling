#pragma once

#include <ser/config.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <type_traits>

#define SER_BYTE_ARRAY(T) ::ser::detail::byte_array_t<T>

namespace ser::detail {

	template<class T>
	using byte_array_t = ::std::array<::std::byte, sizeof(T)>;

	template<class T>
	constexpr void store(::std::byte* dst, const T& v) noexcept {
		static_assert(
			::std::is_trivially_copyable_v<T>,
			"ser::detail::store requires a trivially copyable type"
		);
		if consteval {
			const auto arr = SER_BIT_CAST(SER_BYTE_ARRAY(T), v);
			for (::std::size_t i = 0; i < sizeof(T); ++i) dst[i] = arr[i];
		} else {
			::std::memcpy(dst, &v, sizeof(T));
		}
	}

	template<class T>
	[[nodiscard]] constexpr T load(const ::std::byte* src) noexcept {
		static_assert(
			::std::is_trivially_copyable_v<T>, "ser::detail::load requires a trivially copyable type"
		);
		if consteval {
			byte_array_t<T> arr{};
			for (::std::size_t i = 0; i < sizeof(T); ++i) arr[i] = src[i];
			return SER_BIT_CAST(T, arr);
		} else {
			byte_array_t<T> arr;
			::std::memcpy(arr.data(), src, sizeof(T));
			return SER_BIT_CAST(T, arr);
		}
	}

	template<class T>
	constexpr void loadInto(T& dst, const ::std::byte* src) noexcept {
		static_assert(
			::std::is_trivially_copyable_v<T>,
			"ser::detail::loadInto requires a trivially copyable type"
		);
		static_assert(
			::std::is_trivially_copy_assignable_v<T>,
			"ser::detail::loadInto requires assignability - a type with const "
			"fields goes through serMake, not serRead"
		);
		if consteval {
			byte_array_t<T> arr{};
			for (::std::size_t i = 0; i < sizeof(T); ++i) arr[i] = src[i];
			dst = SER_BIT_CAST(T, arr);
		} else {
			::std::memcpy(&dst, src, sizeof(T));
		}
	}

	constexpr void copyBytes(::std::byte* dst, const ::std::byte* src, ::std::size_t n) noexcept {
		if consteval {
			for (::std::size_t i = 0; i < n; ++i) dst[i] = src[i];
		} else {
			if (n) ::std::memcpy(dst, src, n);
		}
	}

	constexpr void fillBytes(::std::byte* dst, ::std::byte value, ::std::size_t n) noexcept {
		if consteval {
			for (::std::size_t i = 0; i < n; ++i) dst[i] = value;
		} else {
			if (n) ::std::memset(dst, ::std::to_integer<int>(value), n);
		}
	}

	[[nodiscard]] constexpr ::std::size_t alignUp(::std::size_t n, ::std::size_t a) noexcept {
		return (n + a - 1) & ~(a - 1);
	}

}  // namespace ser::detail
