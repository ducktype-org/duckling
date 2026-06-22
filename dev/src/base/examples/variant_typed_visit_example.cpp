#include <base/extend_cpp/variant_match.hpp>

#include <iostream>

struct result_t {
	std::string name;
	int         integer_v;
};

int main() {
	std::variant<int, bool, char> variant;

	auto res = VISIT_RET(variant,
	                   var,
	                   result_t,
	                   return {
						   .name      = typeid(var).name(),
						   .integer_v = int(var),
					   };);

	std::cout << res.name + std::to_string(res.integer_v) << "\n";
}
