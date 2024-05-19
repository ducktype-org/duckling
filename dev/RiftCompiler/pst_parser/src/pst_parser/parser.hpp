/**
 * @file parser.hpp
 * @brief top level parser interface
 */

#pragma once

#include <lexer/token.hpp>
#include <filesystem/file.hpp>
#include "pst.hpp"

namespace pst {
	PST parse(tokenizer::OwnFile&& td);
	PST parse(const fs::FilePath&);

	void init();
}
