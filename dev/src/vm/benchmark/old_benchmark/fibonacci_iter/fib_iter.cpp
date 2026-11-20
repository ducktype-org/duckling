// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <iostream>

int main() {
	int n;
	int mod;

	std::cin >> n >> mod;

	long long a = 0;
	long long b = 1;
	for (int i = 0; i < n; i++) {
		long long c = (a + b) % mod;
		a           = b;
		b           = c;
	}

	std::cout << a << "\n";
}
