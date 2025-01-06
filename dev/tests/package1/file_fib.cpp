#include <iostream>

constexpr long long MOD = 1e9 + 7;

int main() {
	int n = 0;
	std::cin >> n;

	if (n >= 100'000) {
		std::cerr << "Too large" << std::endl;
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
