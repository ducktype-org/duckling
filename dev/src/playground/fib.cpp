// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <iostream>

long long fib(long long n) { return n <= 1 ? n : (fib(n - 1) + fib(n - 2)) % 8'388'449; }

int main() {
	long long n{};
	std::cin >> n;
	std::cout << fib(n) << '\n';

	return 0;
}
