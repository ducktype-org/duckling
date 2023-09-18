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
