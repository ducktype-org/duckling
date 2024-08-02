#include <base/str_concat.hpp>
#include <base/string_id.hpp>
#include <iostream>

int main() {
	base::StrId str("def");

	// prints: abc4def true
	std::cout << base::strConcat("abc", 4, str, " ", true, "\n");
}
