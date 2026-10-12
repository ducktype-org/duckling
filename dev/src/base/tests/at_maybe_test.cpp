// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/collections/maps.hpp>
#include <base/pointers/ref.hpp>

#include <tester/tester.hpp>

class AtMaybeTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS AtMaybeTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testMapAtMaybe);
		TESTER_ADD_TEST(testVectorMapAtMaybe);
	}

private:
	void testMapAtMaybe() {
		base::HashMap<int, std::string> m;
		m.put(1, "one");
		m.put(2, "two");
		ASSERT_EQUAL(true, m.contains(1));
		ASSERT_EQUAL("one", *(m.atMaybe(1).value()));
		ASSERT_EQUAL("two", *(m.atMaybe(2).value()));
		ASSERT_EQUAL(false, m.atMaybe(3).has_value());
	}

	void testVectorMapAtMaybe() {
		base::VectorMap<int, std::string> m;
		m.put(1, "one");
		m.put(2, "two");
		ASSERT_EQUAL(true, m.contains(1));
		ASSERT_EQUAL("one", *(m.atMaybe(1).value()));
		ASSERT_EQUAL("two", *(m.atMaybe(2).value()));
		ASSERT_EQUAL(false, m.atMaybe(3).has_value());
	}
};

TESTER_COMMON_MAIN("/src/base/tests/");
