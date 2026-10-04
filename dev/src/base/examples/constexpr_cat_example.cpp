// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/comptime/constexpr_cat.hpp>

#include <iostream>
#include <string>

int main() {
	constexpr auto a = CONSTEXPR_CAT("A", "B", "C");
	std::cout << std::string(a.data()) << '\n';
}
