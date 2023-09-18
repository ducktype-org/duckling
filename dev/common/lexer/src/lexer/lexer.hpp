/**
 * @file lexer.hpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#pragma once

#include "token.hpp"

#include <filesystem/file.hpp>

namespace lexer {
	void             init();
	lexer::TokenData tokenizeFile(const fs::FilePath& file, bool dprint = false);
}
