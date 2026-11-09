// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/collections/optional.hpp>
#include <base/pointers/ref.hpp>

#include <cassert>

int main() {
	// Create an empty optional
	base::Optional<int> opt;

	assert(false == opt.has_value());
	// or simply
	assert(true == opt.empty());

	opt = 1;
	assert(1 == *opt);
	assert(1 == opt.value());

	base::Optional<int> opt2(2);
	// Mapping the value, and changing a type!
	assert(2.2 == *opt2.map([](int v) { return v * 1.1; }));

	// --------------------------------------------------

	// base::Optional can also hold a reference!
	std::string                            name = "Duckling";
	base::Optional<base::Ref<std::string>> opt_name(&name);

	opt_name.value()->push_back('!');
	assert("Duckling!" == name);
}
