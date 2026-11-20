// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "display_width.hpp"

#include <unicode/uchar.h>
#include <unicode/unistr.h>

namespace term_ui {
	namespace {
		constexpr UChar32 EMOJI_VARIATION_SELECTOR = 0xFE'0F;

		u64 codePointWidth(UChar32 c) {
			if (c == '\t') return TAB_WIDTH;

			const auto zero_width_categories
				= U_GC_MN_MASK | U_GC_ME_MASK | U_GC_CF_MASK | U_GC_CC_MASK;
			if ((U_GET_GC_MASK(c) & zero_width_categories) != 0) return 0;

			const auto east_asian_width = u_getIntPropertyValue(c, UCHAR_EAST_ASIAN_WIDTH);
			if (east_asian_width == U_EA_WIDE || east_asian_width == U_EA_FULLWIDTH) return 2;
			if (u_hasBinaryProperty(c, UCHAR_EMOJI_PRESENTATION)) return 2;

			return 1;
		}
	}

	u64 displayWidth(std::string_view text) {
		const auto unicode_text = icu::UnicodeString::fromUTF8(
			icu::StringPiece(text.data(), static_cast<i32>(text.size()))
		);

		u64     width    = 0;
		UChar32 previous = 0;
		for (i32 i = 0; i < unicode_text.length(); i = unicode_text.moveIndex32(i, 1)) {
			const UChar32 c = unicode_text.char32At(i);
			if (c == EMOJI_VARIATION_SELECTOR && u_hasBinaryProperty(previous, UCHAR_EMOJI)
			    && codePointWidth(previous) == 1)
				width += 1;
			else
				width += codePointWidth(c);
			previous = c;
		}
		return width;
	}

	std::string expandTabs(std::string_view text) {
		std::string res;
		res.reserve(text.size());
		for (char c: text)
			if (c == '\t')
				res.append(TAB_WIDTH, ' ');
			else
				res.push_back(c);
		return res;
	}
}
