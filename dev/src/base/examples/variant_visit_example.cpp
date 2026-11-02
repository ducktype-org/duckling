// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/extend_cpp/variant_match.hpp>

#include <iostream>

int main() {
	std::variant<int, bool, char> variant;

	VARIANT_VISIT(
		variant,
		VISIT_CASE(int&, i, std::cout << i++),
		VISIT_CASE(bool, b, std::cout << int(b)),
		VISIT_CASE(char, c, std::cout << int(c))
	);

	std::cout << VISIT(variant, aut, return int(aut)) << "\n";
}
