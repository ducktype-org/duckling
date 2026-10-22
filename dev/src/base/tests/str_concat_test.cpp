// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/extend_cpp/stringifyable_enum.hpp>
#include <base/str/str_utils.hpp>

#include <tester/tester.hpp>

// Create a test enum for testing enum stringification
MAKE_STRINGIFYABLE_ENUM(test, u32, TestOperation,
	Uninitialized,
	Assign,
	Add,
	Subtract,
	Multiply
)

class ConcatTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ConcatTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testStrConcat);
		TESTER_ADD_TEST(testToString);
		TESTER_ADD_TEST(testEnumStringification);
	}

	void testStrConcat() {
		std::string res;

		res = base::strConcat("aBd", 1, 5, true, "Inny string");
		assertTrue(res == "aBd15trueInny string", "strConcat returned answer other than expected");

		res = base::strConcat();
		assertTrue(res == "", "strConcat returned answer other than expected");

		res = base::strConcat("", "", "");
		assertTrue(res == "", "strConcat returned answer other than expected");

		res = base::strConcat(1, -87, 123'456'789ull);
		assertTrue(res == "1-87123456789", "strConcat returned answer other than expected");

		res = base::strConcat('A', 'B', 'C');
		assertTrue(res == "ABC", "strConcat returned answer other than expected");

		assertThrows<std::domain_error>(
			[]() { base::strConcat("abacabadaba", nullptr); },
			"strConcat of nullptr did not throw correctly"
		);

		assertThrows<std::domain_error>(
			[]() {
				char* ptr = nullptr;
				base::strConcat("abacabadaba", ptr);
			},
			"strConcat of nullptr did not throw correctly"
		);
	}

	void testToString() {
		// Test basic types
		ASSERT_EQUAL(base::toString(42), "42");
		// Test empty string
		ASSERT_EQUAL(base::toString(""), "");
	}

	void testEnumStringification() {
		// Test enum mixed with other types
		auto result = base::strConcat(
			"Step ", 1, ": ", test::TestOperation::Add, ", Step ", 2, ": ", test::TestOperation::Subtract
		);
		assertTrue(
			result == "Step 1: Add, Step 2: Subtract",
			"Mixed enum and other types stringification failed"
		);

		// Test all enum values at once
		result = base::strConcat(
			test::TestOperation::Uninitialized,
			",",
			test::TestOperation::Assign,
			",",
			test::TestOperation::Add,
			",",
			test::TestOperation::Subtract,
			",",
			test::TestOperation::Multiply
		);
		assertTrue(
			result == "Uninitialized,Assign,Add,Subtract,Multiply",
			"All enum values stringification failed"
		);

		// Test const enum reference
		const auto op = test::TestOperation::Add;
		result        = base::strConcat("Const enum: ", op);
		assertTrue(result == "Const enum: Add", "Const enum reference stringification failed");
	}

	~ConcatTest() override = default;

private:
};

TESTER_COMMON_MAIN("/src/base/tests/");
