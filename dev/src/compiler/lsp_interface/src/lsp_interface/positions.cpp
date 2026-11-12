// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <lsp_interface/positions.hpp>

namespace duck_ls {

	namespace {
		/**
		 * @brief The number of bytes in the UTF-8 character whose first byte is `lead`.
		 *
		 * A byte that can only appear inside a character (`0x80` to `0xbf`) should never be
		 * first, but in broken text it can. It then counts as one byte, so the loops that call
		 * this function always move forward and never get stuck.
		 */
		usize sequenceLength(unsigned char lead) {
			if (lead >= 0xf0) return 4;
			if (lead >= 0xe0) return 3;
			if (lead >= 0xc0) return 2;
			return 1;
		}

		/**
		 * @brief The number of bytes taken by the line break that starts at `index` in `text`,
		 * or `0` when no line break starts there.
		 *
		 * @pre `index < text.size()`
		 */
		usize lineBreakLength(std::string_view text, usize index) {
			if (text[index] == '\n') return 1;
			if (text[index] == '\r')
				return index + 1 < text.size() && text[index + 1] == '\n' ? 2 : 1;
			return 0;
		}
	}

	base::Optional<usize> toCodePointOffset(std::string_view text, const lsp::Position& position) {
		usize byte_index = 0;
		usize code_point = 0;

		// Skip the lines before the requested one, counting every code point including the
		// line breaks, because compiler offsets run over the whole file.
		for (u32 line = 0; line < position.line;) {
			if (byte_index >= text.size()) return {};

			if (const usize line_break = lineBreakLength(text, byte_index); line_break > 0) {
				byte_index += line_break;
				code_point += line_break;
				++line;
				continue;
			}

			byte_index += sequenceLength(static_cast<unsigned char>(text[byte_index]));
			++code_point;
		}

		// Move along the requested line by `position.character` UTF-16 code units.
		u32 remaining = position.character;
		while (remaining > 0 && byte_index < text.size() && lineBreakLength(text, byte_index) == 0) {
			const usize bytes = sequenceLength(static_cast<unsigned char>(text[byte_index]));
			const u32   units = bytes == 4 ? 2 : 1;
			if (units > remaining) break;

			remaining -= units;
			byte_index += bytes;
			++code_point;
		}

		return code_point;
	}
}