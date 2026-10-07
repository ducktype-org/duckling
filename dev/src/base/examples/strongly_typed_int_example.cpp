// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/extend_cpp/strongly_typed_int.hpp>

#include <iostream>

STRONG_TYPEDEF_INT(MyInt, int);
STRONG_TYPEDEF_INT_DIMENSIONAL(Kg, int);

int main() {
	MyInt value = MyInt(0);  // ok
	value += MyInt(2);       // ok
	value *= MyInt(2);       // ok

	Kg weight = Kg(0);
	weight += Kg(2);  // ok
	weight *= 2;      // ok
	// weight *= weight; // error

	int raw_value = int(weight);  // ok, explicit
	std::ignore   = raw_value;    // Read for the cpp-linter
	raw_value     = int(value);   // ok, explicit
	std::cout << raw_value << '\n';
}
