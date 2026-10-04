#pragma once

#include <base/types/ints.hpp>

#include <algorithm>

namespace base {
	/**
	 * @brief Struct that allows a string to be passed through a template
	 */
	template<usize N>
	struct TemplateStringLiteral {
		// NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
		constexpr TemplateStringLiteral(const char (&str)[N]): value() {
			std::copy_n(str, N, value);
		}

		char value[N];
		// NOLINTEND(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
	};
}
