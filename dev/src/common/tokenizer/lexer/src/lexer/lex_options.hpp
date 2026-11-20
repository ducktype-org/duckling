// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file lex_options.hpp
 * @brief Options controlling how the lexer turns source characters into tokens.
 */
#pragma once

namespace lexer {

	/**
	 * @brief Options controlling lexer behavior.
	 *
	 * The defaults describe what the compiler front-end needs: only the tokens that carry
	 * meaning. Tools that reproduce the source faithfully, such as the formatter, opt into more.
	 *
	 * @note The options are forwarded through TokenSource::tokenize() to the Lexer and are not
	 *       stored on the TokenSource, so every tokenization picks its own.
	 */
	struct LexOptions final {
		/**
		 * Whether comment tokens are kept in the output stream. Comments are discarded by
		 * default, as the parser has no use for them.
		 */
		bool keep_comments = false;
	};
}
