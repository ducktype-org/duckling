// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/str/str_utils.hpp>

#include <string_id/string_id.hpp>

#include <iostream>

int main() {
	base::StrID str("def");

	// prints: abc4def true
	std::cout << base::strConcat("abc", 4, str, " ", true, "\n");
}
