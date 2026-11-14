// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <lsp/types.h>

#include <token_source/source.hpp>

namespace duck_ls {
	/**
	 * @brief The LSP `character`, in UTF-16 code units, of the position after the first
	 * @p code_points code points of the 1-based @p line of @p source.
	 *
	 * The compiler counts columns in code points while the protocol counts them in UTF-16 code
	 * units, where a code point above U+FFFF takes two. A code point past the end of the source
	 * counts as one unit, and a line @p source does not have leaves @p code_points unchanged.
	 */
	lsp::Uint codePointsToUtf16Units(
		const tokenizer::TokenSource& source, usize line, usize code_points
	);
}
