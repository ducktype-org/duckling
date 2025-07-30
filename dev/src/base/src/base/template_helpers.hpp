#pragma once

#include <cstddef>
#include <algorithm>

namespace base {
	/**
	 * @brief Struct that allows a string to be passed through a template
	 */
	template<std::size_t N>
	struct TemplateStringLiteral {
    	constexpr TemplateStringLiteral(const char (&str)[N]) {
        	std::copy_n(str, N, value);
    	}
    
    	char value[N];
	};
}