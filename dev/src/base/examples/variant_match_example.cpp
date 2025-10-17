#include <base/extend_cpp/variant_match.hpp>

int main() {
	std::variant<int, bool, char> variant;

	variant_match(variant) {
		variant_case(int, v_i) {
			// use v_i as int
		}
		variant_case_novalue(char) {
			// do some stuff if variant holds char
		}
		variant_default {
			// executes if non other does
		}
	}
}
