#include <base/except/exceptions.hpp>

#include <iostream>

int main() {
	std::cout << base::getCurrentStackTrace() << '\n';
	return 0;
}
