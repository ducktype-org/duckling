// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/extend_cpp/variant_match.hpp>

int main() {
	std::variant<int, bool, char> variant;

	variant_match(variant) {
		variant_case(int, v_i) {
			// use v_i as int
		}
		variant_case_novalue(char) {
			// do some stuff if variant holds char
		}
		variant_default {
			// executes if non other does
		}
	}
}
