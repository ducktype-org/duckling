/**
 * @file parser.hpp
 * @brief top level parser interface
 */

#pragma once

#include "pst.hpp"
#include <filesystem/file.hpp>
#include <lexer/token.hpp>

namespace pst {
	PST parse(lexer::TokenData&& td);
	PST parse(const fs::FilePath&);

	void init();
}
