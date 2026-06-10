#include <iostream>

long long fib(long long n) { return n <= 1 ? n : (fib(n - 1) + fib(n - 2)) % 8'388'449; }

int main() {
	long long n{};
	std::cin >> n;
	std::cout << fib(n) << '\n';

	return 0;
}
