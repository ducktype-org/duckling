// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/extend_cpp/stringifyable_enum.hpp>
#include <base/str/str_utils.hpp>

#include <lang_definitions/key_spec_op.hpp>
#include <string_id/string_id.hpp>

#include <cstdint>
#include <string>

/**
 * @brief Whether a view type (`ref`, `ptr`, `manyptr`, etc.) can write to what it points to.
 *
 * @note Only a view type have it. It's a separate thing from `PlaceAccess`.
 * For `let a: ref i32 = &x;`, The place `a` has `PlaceAccess::Immutable` (the reference cannot be
 * rebound) and stores `ViewAccess::ReadOnly` (you can't write through `a`).
 */
MAKE_STRINGIFYABLE_ENUM(compiler::tsh, uint8_t, ViewAccess,
	ReadOnly,  ///< The place may only be read.
	ReadWrite  ///< The place may be read and written.
)

namespace compiler::tsh {
	/**
	 * @brief Creates a string representation of a view type (its keyword + an optional `mut`).
	 */
	[[nodiscard]]
	inline std::string viewOperator(const lang_def::Keyword view, const ViewAccess access) {
		const base::StrID op = access == ViewAccess::ReadWrite
		                         ? lang_def::writableViewOperator(view).value()
		                         : lang_def::keywordToStr(view);
		return base::strConcat(op.strView(), " ");
	}
}
