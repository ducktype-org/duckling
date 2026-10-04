// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
