#pragma once

#include "ints.hpp"
#include <array>

namespace base {

	// Constexpr concatenation of char, string literal and char sequence types
	// such as std::array<char> with constexpr size() and data() free functions

	namespace impl {
		// Implement free size() and data() for char
		// Specialize size() for string literal - override to ignore null terminator
		// constexpr copy_n for char sequences

		// size(char) -> 1 overload to complement std::size
		constexpr usize size(const char&) {
			return 1U;
		}

		// data(char) -> char* overload to complement std::data
		constexpr const char* data(const char& c) {
			return &c;
		}

		// size(char[N]) overrides std::size(char[N]) as more specialized
		// ***Assumes char[N] is string literal*** ***STRIPS ZERO TERMINATOR***
		template<usize N>
		constexpr usize size(const char (&)[N]) {
			return N - 1;
		}

		constexpr char* copy_n(const char* cs, usize n, char* p) {
			for (usize i = 0; i < n; i++) *(p + i) = *(cs + i);
			return p + n;
		}

		template<typename... Cs>
		constexpr usize sizeSum(const Cs&... cs) {
			return (0U + ... + size(cs));
		}
	}

	// Returns std::array<char> concatenation of the input character sequences
	template<usize SIZE, typename... Cs>
	constexpr auto cat(const Cs&... cs) {
		using impl::data;
		using impl::size;
		using std::data;
		std::array<char, SIZE> ret{ {} };
		if constexpr (ret.size()) {
			char* p = ret.data();
			((p = impl::copy_n(data(cs), size(cs), p)), ...);
		}
		return ret;
	}
}

#define CONSTEXPR_CAT(...) base::cat<base::impl::sizeSum(__VA_ARGS__)>(__VA_ARGS__);
