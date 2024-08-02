#include <base/variant.hpp>
#include <iostream>

int main() {
	std::variant<int, bool, char> variant;

	std::cout << VARIANT_VISIT(
		variant,
		VARIANT_CASE(int&, i, return i++) VARIANT_CASE(bool, b, return int(b))
			VARIANT_CASE(char, c, return int(c))
	) << "\n";

	std::cout << VISIT(variant, aut, return int(aut)) << "\n";
}
