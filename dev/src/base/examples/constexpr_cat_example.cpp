#include <base/comptime/constexpr_cat.hpp>

#include <iostream>
#include <string>

int main() {
	constexpr auto a = CONSTEXPR_CAT("A", "B", "C");
	std::cout << std::string(a.data()) << '\n';
}
