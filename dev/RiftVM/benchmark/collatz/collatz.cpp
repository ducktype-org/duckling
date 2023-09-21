#include <algorithm>
#include <iostream>

long long collatz(long long n) {
	long long j = 0;
	while (n != 1) {
		if (n % 2 == 0)
			n /= 2;
		else
			n = n * 3 + 1;
		if (n > j) j = n;
	}
	return j;
}

int main() {
	int       x;
	long long s = 0;

	std::cin >> x;

	for (int i = 1; i < x; i++) s = std::max(s, collatz(i));

	std::cout << s << "\n";
}
