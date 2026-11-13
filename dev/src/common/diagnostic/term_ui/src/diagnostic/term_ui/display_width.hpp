// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/types/ints.hpp>

#include <string>
#include <string_view>

namespace term_ui {
	/**
	 * @brief The number of terminal columns a tab is expanded to.
	 */
	constexpr u64 TAB_WIDTH = 4;

	/**
	 * @brief The number of terminal columns the UTF-8 @p text occupies when printed.
	 *
	 * Wide and fullwidth characters (East Asian Width W/F, emoji presentation) take 2 columns,
	 * combining marks, format and control characters take 0 and a tab takes @ref TAB_WIDTH.
	 * An emoji variation selector widens a preceding narrow emoji to 2 columns. Every other code
	 * point, including the replacement of invalid UTF-8, takes 1. Widths are summed per code
	 * point, so multi code point emoji sequences are wider than most terminals draw them.
	 */
	u64 displayWidth(std::string_view text);

	/**
	 * @brief Replaces every tab in @p text with @ref TAB_WIDTH spaces, so the printed width
	 * matches @ref displayWidth regardless of the terminal tab stops.
	 */
	std::string expandTabs(std::string_view text);
}
