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
