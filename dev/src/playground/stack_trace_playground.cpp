#include <base/except/exceptions.hpp>
#include <query_framework/query_errors.hpp>
#include <query_framework/internal/query_errors.hpp>

#include <iostream>

void rec(int n) {
	if (n > 0) {
		rec(n - 1);
	} else {
		query::throwFailed("abc");
	}
}

int main() {
	int n = 0;
	std::cin >> n;
	try {
		rec(n);
	} catch (const query::internal::QueryFailedException &e) {
		// std::cout << "Caught exception: " << e.what() << "\n";
		// std::cout << n << "\n";
	}
	return 0;
}
