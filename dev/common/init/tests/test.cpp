/**
 * Due to the nature of the test,
 * we are not using Tester framework here.
 */

#include <init/init.hpp>
#include <base/exceptions.hpp>
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

REGISTER_FUNC_FOR_DEINIT(testFunction(1));
REGISTER_FUNC_FOR_DEINIT(testFunction(2));
REGISTER_FUNC_FOR_DEINIT(testFunction(3));

namespace some_namespace {
	REGISTER_FUNC_FOR_DEINIT(testFunction(4));
}

namespace {
	REGISTER_FUNC_FOR_DEINIT(testFunction(5));
}

int main() {
	deinit::registerForDeinit(testFunction(6));
	deinit::registerForDeinit([&]() {
		CORE_ASSERT(counter == 6, "Test failed at the end.");
		std::cout << "Test passed!\n";
	});
}
