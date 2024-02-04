/**
 * @file parser.hpp
 * @brief top level parser interface
 */

#pragma once

#include <lexer/token.hpp>
#include <filesystem/file.hpp>
#include "pst.hpp"

namespace pst {
	PST parse(lexer::TokenData&& td);
	PST parse(fs::FilePath);

	void init();
}
