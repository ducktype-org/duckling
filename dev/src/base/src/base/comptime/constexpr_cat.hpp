/**
 * @file constexpr_cat.hpp
 * @brief Constexpr concatenation of char, string literal and char sequence types.
 *
 * @note For a char sequence to work it needs constexpr size() and data() methods.
 *
 * Constexpr cat implements a way to concatenate strings during compile time.
 * Is is extremely specific and strange, and should not be used under standard
 * circumstances.
 *
 * @note It is currently only used by Json library.
 * @note For concatenation at runtime use `base::strConcat`
 *
 * Usage:
 * @include constexpr_cat_example.cpp
 *
 * @example constexpr_cat_example.cpp
 *
 * @date 2024-03-28
 */
#pragma once

#include <base/types/ints.hpp>

#include <array>

namespace base {
	namespace impl {
		// Implement free size() and data() for char
		// Specialize size() for string literal - override to ignore null terminator
		// constexpr copy_n for char sequences

		// size(char) -> 1 overload to complement std::size
		constexpr usize mySize(const char&) { return 1; }

		// NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
		template<usize N>
		constexpr usize mySize(const char (&)[N]) {
			return N - 1;
		}

		// NOLINTEND(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)

		template<usize N>
		constexpr usize mySize(const std::array<char, N>& arr) {
			usize s = N;
			while (s && arr[s - 1] == '\0') s--;
			return s;
		}

		// data(char) -> char* overload to complement std::data
		constexpr const char* data(const char& c) { return &c; }

		/**
		 * @brief Copies `n` bytes between `from` and `to`. Returns `to` + `n`
		 */
		constexpr char* copyN(const char* from, usize n, char* to) {
			for (usize i = 0; i < n; i++) *(to + i) = *(from + i);
			return to + n;
		}

		template<typename... Cs>
		constexpr usize sizeSum(const Cs&... cs) {
			return (mySize(cs) + ...);
		}
	}

	/**
	 * @brief Returns std::array<char> concatenation with supplied length.
	 *
	 * @note Unsafe when wrong length supplied.
	 */
	template<usize SIZE, typename... Cs>
	constexpr std::array<char, SIZE> cat(const Cs&... cs) {
		using impl::data;
		using impl::mySize;
		using std::data;
		std::array<char, SIZE> ret{ {} };
		if constexpr (ret.size()) {
			char* p = ret.data();
			((p = impl::copyN(data(cs), mySize(cs), p)), ...);
		}
		return ret;
	}
}

/**
 * @brief Returns a std::array<char> concatenation with inferred length.
 * @note std::array is not null-terminated!
 */
#define CONSTEXPR_CAT(...) base::cat<base::impl::sizeSum(__VA_ARGS__)>(__VA_ARGS__)

/**
 * @brief Returns a null-terminated std::array<char> concatenation with inferred length.
 * It is safe to e.g. use .data()'s content in streams
 */
#define CONSTEXPR_CAT_CSTR(...) CONSTEXPR_CAT(__VA_ARGS__, '\0')
