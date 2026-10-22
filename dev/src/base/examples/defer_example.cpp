// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/extend_cpp/defer.hpp>

int main() {
	int a = 3;
	// a = 3
	{
		// a = 3
		a += 2;
		// a = 5
		defer(a++);
		// a = 5
		a--;
		// a = 4
	}
	// a = 5
	{
		defer(a = 4);
		defer(a--);
		// a = 5
	}
	// a = 4;
	{
		defer(a--);
		defer(a = 3);
		// a = 4
	}
	// a = 2
}
