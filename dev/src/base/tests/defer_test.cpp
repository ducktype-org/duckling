// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/extend_cpp/defer.hpp>

#include <tester/tester.hpp>

class DeferTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DeferTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(deferTest); }

private:
	void deferTest() {
		i32 a = 0;
		{ defer(a = 1); }
		assertTrue(a == 1, "Defer didn't execute or didn't capture variable");

		i32 b = 0;
		i32 c = 0;
		{
			c = 100;
			defer({
				b = 1;
				c = 2;
			});
			c = 100;
		}
		assertTrue(b == 1, "Defer didn't execute after all other statements (1)");
		assertTrue(c == 2, "Defer didn't execute after all other statements (2)");

		{
			defer(a = 3);
			defer(a = 2);
			a = 4;
		}
		assertTrue(a == 3, "Defer didn't execute in correct order");
	}
};

TESTER_COMMON_MAIN("/src/base/tests/");
