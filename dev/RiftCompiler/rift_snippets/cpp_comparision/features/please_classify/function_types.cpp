#include <functional>
#include <iostream>

template<typename T, typename U>
std::vector<U> map(std::vector<T> list, std::function<U(const T&)> modifier) {
	std::vector<U> result;
	for(const T& el: list) {
		result.push_back(modifier(el));
	}
	return result;
}

template<typename T>
std::vector<T> filter(std::vector<T> list, std::function<bool(const T&)> condition) {
	std::vector<T> result;
	for(const T& el: list) {
		if (condition(el)) {
			result.push_back(el);
		}
	}
	return result;
}

// int main() {
	// std::vector<int> list = {1, 2, 3, 4, 5};

	// std::vector<double> mapped = map(list, std::function{[](const int& el) -> double{return 1.5 * el;}});

	// std::vector<int> filtered = filter(list, std::function{[](const int& el) -> bool{return el % 2 == 1;}});

	// std::cout << "Initial list: ";
	// for(const auto& el: list) {
		// std::cout << el << " ";
	// }
	// std::cout << "\nMapped list: ";
	// for(const auto& el: mapped) {
		// std::cout << el << " ";
	// }
	// std::cout << "\nFiltered list: ";
	// for(const auto& el: filtered) {
		// std::cout << el << " ";
	// }
	// std::cout << "\n";
	// return 0;
// }
