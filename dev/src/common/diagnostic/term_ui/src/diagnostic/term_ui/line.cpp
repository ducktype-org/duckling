// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "line.hpp"

#include <diagnostic/term_ui/code_section.hpp>
#include <diagnostic/term_ui/styles.hpp>

namespace term_ui {

	LinePiece::LinePiece(std::string text, StyleType type): text(std::move(text)), type(type) {}

	const std::string LinePiece::getText() const { return text; }

	void LinePiece::print(std::ostream& out) const { getStyleFromType(type).printWith(text, out); }

	bool Line::isEmptyOn(u64 beg, u64 len) const {
		u64 end = beg + len;
		for (auto& [col, piece]: pieces) {
			u64 col_end = col + piece.getText().size();
			if (col < beg && col_end <= beg) continue;
			if (beg < col && end <= col) {
				// Pieces are sorted.
				return true;
			}
			return false;
		}
		return true;
	}

	bool Line::tryInsert(u64 beg, const LinePiece& piece) {
		if (!isEmptyOn(beg, piece.getText().size())) return false;
		pieces.put(beg, piece);
		return true;
	}

	void Line::print(std::ostream& out) const {
		u64 col = 0;

		for (auto& [beg, piece]: pieces) {
			// Fill the gap with empty characters.
			if (beg > col) {
				auto fill = std::string(beg - col, ' ');
				out << fill;
			}
			// Print the piece.
			piece.print(out);
			col = beg + piece.getText().size();
		}
		out << '\n';
	}
}
