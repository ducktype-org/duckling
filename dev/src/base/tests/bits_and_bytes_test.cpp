// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/types/bits_and_bytes.hpp>

#include <tester/tester.hpp>

using base::bytes2bits;

class BitsAndBytesTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS BitsAndBytesTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(bitsAndBytesTest); }

	template<typename B>
	void unitTest() {
		B n1(1);
		assertTrue(usize(n1) == 1, "Basic math failed (1)");
		assertTrue(n1 == B(1), "Basic math failed (2)");

		B n2(2);
		B n3 = n1 + n2;
		assertTrue(n3 == B(3), "Basic math failed (3)");
		assertTrue(n3 != B(1), "Basic math failed (4)");
		assertTrue(n3 > B(1), "Basic math failed (5)");
		assertTrue(n3 > B(2), "Basic math failed (6)");
		assertTrue(n3 >= B(3), "Basic math failed (7)");
		assertTrue(n3 <= B(3), "Basic math failed (8)");
		assertTrue(n3 < B(4), "Basic math failed (9)");
		assertTrue(n3 < B(10), "Basic math failed (10)");

		assertTrue(B(7) - n3 == B(4), "Basic math failed (11)");
		assertTrue(n3 * 2 == B(6), "Basic math failed (12)");
		assertTrue(n3 / 2 == B(1), "Basic math failed (13)");

		n3 += B(10);
		assertTrue(n3 == B(13), "Basic math failed (14)");
		n3 -= B(9);
		assertTrue(n3 == B(4), "Basic math failed (15)");
	}

	void bitsAndBytesTest() {
		// Basic maffs.
		unitTest<Bits>();
		unitTest<Bytes>();

		// Additional functionalities test.
		Bytes bytes = Bytes(3);
		Bits  bits  = bytes2bits(bytes);
		assertTrue(usize(bits) == 3 * 8, "Conversion failed");

		assertTrue(base::toString(bytes) == "3B", "Byte stringification failed");
		assertTrue(base::toString(bits) == "24b", "Bit stringification failed");
	}

	~BitsAndBytesTest() override = default;
};

TESTER_COMMON_MAIN("/src/base/tests/");
