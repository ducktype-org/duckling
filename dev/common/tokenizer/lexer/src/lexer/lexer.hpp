/**
 * @file lexer.hpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#pragma once

#include <filesystem/file.hpp>
#include "token.hpp"

namespace lexer {
	/**
	 * @brief Initializes the whole module
	 */
	void             init();

	/**
	 * @brief Decodes and splits the input file into Tokens.
	 * 
	 * If either of those fails the errors are printed to the standard error output and `base::LogicError` is thrown.
	 * 
	 * @param file File to tokenize
	 * @return lexer::TokenData Containing the Tokens
	 */
	lexer::TokenData tokenizeFile(const fs::FilePath& file);
}
