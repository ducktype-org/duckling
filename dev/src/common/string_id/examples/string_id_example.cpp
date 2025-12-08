#include <base/str/str_utils.hpp>

#include <string_id/string_id.hpp>

#include <iostream>

int main() {
	base::StrID str("def");

	// prints: abc4def true
	std::cout << base::strConcat("abc", 4, str, " ", true, "\n");
}
