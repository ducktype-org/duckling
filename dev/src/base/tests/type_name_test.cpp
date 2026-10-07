// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/comptime/type_traits.hpp>

#include <tester/tester.hpp>

class T {};

class SomeLongName {};

namespace n {
	class SomeLongName {};
}

class TypeNameTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS TypeNameTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(simpleTest); }

	void simpleTest() {
		assertTrue(base::typeName<int>() == "int", "int");
		assertTrue(base::typeName<T>() == "T", "T");
		assertTrue(base::typeName<SomeLongName>() == "SomeLongName", "SomeLongName");

		// note: this one might be compiler dependent.
		// if it breaks in the future for that reason, feel free to relax it
		assertTrue(base::typeName<n::SomeLongName>() == "n::SomeLongName", "n::SomeLongName");
	}
};

TESTER_COMMON_MAIN("/src/base/tests");
