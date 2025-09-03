#pragma once

#include "ints.hpp"

#include <algorithm>

namespace base {
	/**
	 * @brief Struct that allows a string to be passed through a template
	 */
	template<usize N>
	struct TemplateStringLiteral {
		constexpr TemplateStringLiteral(const char (&str)[N]): value() {
			std::copy_n(str, N, value);
		}

		char value[N];
	};
}
