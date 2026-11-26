// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <lsp/types.h>

#include <base/collections/optional.hpp>
#include <base/types/ints.hpp>

#include <diagnostic/source_position.hpp>
#include <token_source/source.hpp>

#include <string_view>

namespace duck_ls {

	/**
	 * @brief Converts an LSP client position to the compiler's offset in the `text` document.
	 *
	 * The client gives a line and a column. Lines are counted from zero, and a line ends with
	 * `\n`, `\r\n` or `\r` (all three are line breaks in LSP). The column is counted in UTF-16
	 * code units, the encoding `ServerSession::position_encoding` reports in `initialize`.
	 *
	 * The compiler counts code points from the start of the file, and line breaks are code
	 * points too: `\n` and `\r` are one code point each, `\r\n` is two. This is the same count
	 * as `dia::SourcePosition::getStart()`. The result is the number of code points before the
	 * position.
	 *
	 * A column past the end of its line is treated as the end of that line. A column that falls
	 * between the two UTF-16 code units of one character is treated as the start of that
	 * character.
	 *
	 * @param text The whole document in UTF-8, the same version the compiler parsed.
	 * @param position The position the client sent.
	 * @return Empty when `text` has no such line.
	 */
	base::Optional<usize> toCodePointOffset(std::string_view text, const lsp::Position& position);

	/**
	 * @brief Converts an LSP client position to a byte index in the `text` document.
	 *
	 * Finds the same place as `toCodePointOffset`, by the same rules, but counts the UTF-8 bytes
	 * before it instead of the code points.
	 *
	 * @param text The whole document in UTF-8.
	 * @param position The position the client sent.
	 * @return Empty when `text` has no such line.
	 */
	base::Optional<usize> toByteOffset(std::string_view text, const lsp::Position& position);

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

	/**
	 * @brief The LSP range of @p position, the place a node takes in the source.
	 */
	lsp::Range toLspRange(const dia::SourcePosition& position);
}
