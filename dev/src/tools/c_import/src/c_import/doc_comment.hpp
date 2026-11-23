// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file doc_comment.hpp
 * @brief Turning C doc comments into plain text.
 */
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace c_import {

	/**
	 * @brief The text of a C doc comment, in any of Doxygen's block, line or trailing forms,
	 * without its markers and common indentation, as valid UTF-8.
	 */
	std::string cleanDocComment(std::string_view raw);

	/**
	 * @brief The doc comment of the macro defined on 1-based `line` of `source`: one trailing on
	 * that line, or one ending right above it.
	 */
	std::string macroDocComment(std::string_view source, std::size_t line);

}
