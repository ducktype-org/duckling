// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * Due to the nature of the test,
 * we are not using Tester framework here.
 */

#include <base/except/exceptions.hpp>

#include <init/init.hpp>

#include <iostream>

namespace {
	int counter = 0;
}

auto testFunction(int val) {
	return [val]() {
		counter += 1;
		CORE_ASSERT(counter == val, "Test failed for val: ", val);
	};
}

RUN_BEFORE_MAIN(testFunction(1)());
RUN_BEFORE_MAIN(testFunction(2)());

RUN_BEFORE_MAIN(init::registerForInit(testFunction(4)));

namespace some_namespace {
	RUN_BEFORE_MAIN(testFunction(3)());
}

namespace {
	RUN_BEFORE_MAIN(init::registerForInit(testFunction(5)));
}

RUN_BEFORE_MAIN(init::registerForDeinit(testFunction(6)));

int main() {
	init::InitObject _;

	init::registerForDeinit(testFunction(7));
	init::registerForDeinit([&]() {
		CORE_ASSERT(counter == 7, "Test failed at the end.");
		std::cout << "Test passed!\n";
	});
}
