// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once
#include "styles.hpp"

#include <base/collections/maps.hpp>

#include <iostream>

namespace term_ui {

	/**
	 * @brief Piece of formatted text to be placed in a line.
	 *
	 */
	class LinePiece {
		// The text to be placed.
		std::string text;
		// The style of the text.
		StyleType type;

	public:
		// Construct a line piece from its field values.
		LinePiece(std::string text, StyleType type);

		// Get the text.
		[[nodiscard]] const std::string getText() const;

		/**
		 * @brief Print the line piece to the output stream
		 * using its style.
		 *
		 * @param out The output stream.
		 */
		void print(std::ostream& out) const;
	};

	/**
	 * @brief Line of formatted text.
	 *
	 * This class provides a layer of abstraction above simply updating
	 * individual characters of a line string. Instead, it provides a method
	 * of inserting new line piece at specific indices (but only if they fit)
	 * and later printing the entire line with whitespace filling between
	 * the line pieces.
	 */
	class Line {
		// The list of non-overlapping line pieces mapped by their starting
		// position in the line.
		base::Map<u64, LinePiece> pieces;

	public:
		// Check if the line is empty on interval [beg, beg + len - 1].
		[[nodiscard]] bool isEmptyOn(u64 beg, u64 len) const;

		// Try to insert a new line piece into this line starting on column beg.
		// Return true on success and false on failure.
		bool tryInsert(u64 beg, const LinePiece& piece);

		/**
		 * @brief Print the entire line to the output stream, inserting
		 * whitespace in gaps between line pieces.
		 *
		 * @param out The output stream.
		 */
		void print(std::ostream& out) const;
	};
}
