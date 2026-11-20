// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <algorithm>
#include <iostream>

const int m = 8'388'449;

int fib(int n) {
	if (n <= 1) return n;
	return (fib(n - 1) + fib(n - 2)) % m;
}

int main() {
	int n;
	std::cin >> n;
	std::cout << fib(n) << "\n";
}
