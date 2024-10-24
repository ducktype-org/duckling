#include <tuple>
#include <iostream>

int main() {
	std::tuple<int, double, std::string> tuple = {1, 2.5, "hello"};

	std::cout << std::get<0>(tuple) << "\n";
	std::cout << std::get<1>(tuple) << "\n";
	std::cout << std::get<2>(tuple) << "\n";

	return 0;
}

// @TODO: function tuple return (multi value return)
// tuples inside sets/maps/sorts for comparisons

