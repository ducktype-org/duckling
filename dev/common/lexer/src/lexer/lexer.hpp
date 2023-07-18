/**
 * @file lexer.hpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#pragma once

#include <filesystem/file.hpp>
#include "token.hpp"

namespace lexer {
	void init();
	lexer::TokenData tokenizeFile(fs::FileContent file, bool dprint = false);
}



