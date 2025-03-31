#include <iostream>

#include <base/str_utils.hpp>
#include <base/string_id.hpp>

int main() {
	base::StrID str("def");

	// prints: abc4def true
	std::cout << base::strConcat("abc", 4, str, " ", true, "\n");
}
