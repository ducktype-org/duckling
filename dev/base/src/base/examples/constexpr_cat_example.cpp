#include <base/constexpr_cat.hpp>
#include <iostream>

int main() {
	constexpr a = CONSTEXPR_CAT("A", "B", "C");
	std::cout << a;
}
