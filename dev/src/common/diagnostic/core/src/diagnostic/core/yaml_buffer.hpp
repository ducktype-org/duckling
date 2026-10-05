// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <streambuf>

class string_view_streambuf: public std::streambuf {
public:
	string_view_streambuf(std::string_view sv) {
		char* begin = const_cast<char*>(sv.data());  // NOLINT
		char* end   = begin + sv.size();
		this->setg(begin, begin, end);
	}
};
