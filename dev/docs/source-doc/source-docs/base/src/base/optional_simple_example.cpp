#include <base/optional.hpp>
#include <cassert>

int main() {
	// Create an empty optional
	base::Optional<int> opt;

	ASSERT_EQUAL(false, opt.has_value());
	// or simply
	ASSERT_EQUAL(true, opt.empty());

	opt = 1;
	ASSERT_EQUAL(1, *opt);
	ASSERT_EQUAL(1, opt.value());

	base::Optional<int> opt2(2);
	// Mapping the value, and changing a type!
	ASSERT_EQUAL(2.2, *opt2.map([](int v) { return v * 1.1; }));

	// --------------------------------------------------

	// base::Optional can also hold a reference!
	std::string                  name = "Duckling";
	base::Optional<std::string&> opt_name(name);

	opt_name.value().push_back('!');
	ASSERT_EQUAL("Duckling!", name);
}
