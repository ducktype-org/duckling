#pragma once

#include <algorithm>
#include <cstddef>

namespace base {
	/**
	 * @brief Struct that allows a string to be passed through a template
	 */
	template<std::size_t N>
	struct TemplateStringLiteral {
		constexpr TemplateStringLiteral(const char (&str)[N]): value() { std::copy_n(str, N, value); }

		char value[N];
	};
}
