// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <lsp_interface/positions.hpp>

namespace duck_ls {
	namespace {
		constexpr UChar32 LAST_SINGLE_UNIT_CODE_POINT = 0xFF'FF;
	}

	lsp::Uint codePointsToUtf16Units(
		const tokenizer::TokenSource& source, usize line, usize code_points
	) {
		if (line == 0 || line > source.getLines().size())
			return base::safeIntConv<lsp::Uint>(code_points);

		const auto& chars = source.getChars();
		const usize begin = source.getLine(line).first;

		lsp::Uint units = 0;
		for (usize i = begin; i < begin + code_points; ++i)
			units += i < chars.size() && chars[i].value > LAST_SINGLE_UNIT_CODE_POINT ? 2 : 1;
		return units;
	}
}
