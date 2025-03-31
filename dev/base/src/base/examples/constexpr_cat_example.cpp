#include <iostream>
#include <string>

#include <base/constexpr_cat.hpp>

int main() {
	constexpr auto a = CONSTEXPR_CAT("A", "B", "C");
	std::cout << std::string(a.data()) << std::endl;
}
