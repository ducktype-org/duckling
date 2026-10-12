// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/misc/anycast.hpp>

#include <iostream>

int main() {
	std::any x = std::string{ "123" };

	std::cout << base::anyCast<std::string>(x);
	// 123

	std::cout << base::anyCast<int>(x);
	/*
	    terminate called after throwing an instance of 'base::LogicError'
	    what():  Bad any_cast: Value is of different type than "int"
	*/
}
