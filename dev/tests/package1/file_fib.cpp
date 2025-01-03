#include <iostream>

int main() {
	int n = 0;
	std::cin >> n;

	if (n >= 100'000) {
		std::cerr << "Too large" << std::endl;
		return 1;
	}

	int a = 0, b = 1;
	while (n--) {
		std::cout << a << " ";
		b += a;
		a = b - a;
	}
}
