// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <iostream>

constexpr long long MOD = static_cast<long long>(1e9) + 7;

int main() {
	int n = 0;
	std::cin >> n;

	if (n >= 100'000) {
		std::cerr << "Too large\n";
		return 1;
	}

	long long a = 0, b = 1;
	while (n--) {
		long long ans = ((a % MOD) + MOD) % MOD;
		std::cout << ans << " ";
		b = (a + b) % MOD;
		a = (b - a) % MOD;
	}
}
