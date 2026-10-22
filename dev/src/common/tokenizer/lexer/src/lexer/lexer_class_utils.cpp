// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "lexer_class.hpp"

#include <logger/logger.hpp>

namespace lexer {
	void Lexer::next() {
		if (!isEOF()) {
			if (isEOL()) {
				// handling of CR+LF as one newline
				if (peek().is(0x0D) && peek(1).is(0x0A)) where++;
				where++;
			} else {
				where++;
			}
		} else {
			throw base::LogicError("Tried to skip EOF");
		}
	}

	void Lexer::skip(usize n) {
		for (usize i = 0; i < n; i++) next();
	}

	bool Lexer::tryRawValue(char raw_value, usize fwd) const {
		return char_array.size() > where + fwd && peek(fwd).is(raw_value);
	}

	const Char& Lexer::peek(usize fwd) const { return char_array.at(where + fwd); }

	std::string Lexer::generateLineColumnInfo(usize fwd) const {
		auto [line, column] = file->getLineColumn(where + fwd);
		return "(" + std::to_string(line) + ":" + std::to_string(column) + ")";
	}

	void Lexer::addTokenMsg(usize begin, usize end, std::string_view token_type) {
		CORE_DEV_LOG(
			Lexer,
			"Add token: ",
			std::string(token_type),
			"(",
			std::string(file->getCharRange(begin, end + 1).stringView()),
			")\n"
		)
	}

	void Lexer::parseUntil(Tokens& output, const LexerCondition& stop) {
		while (!stop(*this)) parseSingleInto(output);
	}

	dia::SourcePosition Lexer::currentPosition() const { return { file->getLocation(), where }; }
}
