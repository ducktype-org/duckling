/**
 * @file lexer.hpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#pragma once

#include <base/box.hpp>

#include <filesystem/file.hpp>
#include <token_source/forward.hpp>

namespace lexer {
	/**
	 * @brief Decodes and splits the input file into Tokens.
	 *
	 * If either of those fails the errors are printed to the standard error output and
	 * `base::LogicError` is thrown.
	 *
	 * @param file File to tokenize
	 * @return lexer::TokenData Containing the Tokens
	 */
	Box<tokenizer::TokenSource> tokenizeFile(const fs::File& file);
}
