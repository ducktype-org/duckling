// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/except/exceptions.hpp>

#include <thread>

struct T {
	~T() { CORE_ASSERT_NOEXCEPT(false, "panic in panic!"); }
};

int main() {
	try {
		T t;
		CORE_PANIC("Fresh, crispy panic for my dudes <3");
	} catch (const base::Panic& p) { p.printToCerr(); }

	// For hand-testing multi-threaded panics (comment out the lines above to run it):
	std::jthread t1([] { CORE_PANIC("Fresh, crispy panic for my dudes <3"); });
	std::jthread t2([] { CORE_PANIC("Fresh, crispy panic for my dudes <3"); });
	std::jthread t3([] { CORE_PANIC("Fresh, crispy panic for my dudes <3"); });
	std::jthread t4([] { CORE_PANIC("Fresh, crispy panic for my dudes <3"); });
	std::jthread t5([] { CORE_PANIC("Fresh, crispy panic for my dudes <3"); });
}
