// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/collections/dynamic_bitset.hpp>

#include <tester/tester.hpp>

#include <vector>

using base::DynamicBitset;
using Indices = std::vector<usize>;

class DynamicBitsetTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DynamicBitsetTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(basicSetTest);
		TESTER_ADD_TEST(setOpsTest);
		TESTER_ADD_TEST(equalityTest);
		TESTER_ADD_TEST(forEachSetTest);
		TESTER_ADD_TEST(largeCapacityTest);
	}

	static Indices setBits(const DynamicBitset& bs) {
		Indices out;
		bs.forEachSet([&](usize i) { out.push_back(i); });
		return out;
	}

	void basicSetTest() {
		DynamicBitset bs(10);
		assertTrue(bs.size() == 10, "size should be 10");
		assertTrue(not bs.any(), "fresh bitset should be empty");
		assertTrue(bs.count() == 0, "fresh count should be 0");

		bs.set(0);
		bs.set(9);
		assertTrue(bs.test(0), "bit 0 set");
		assertTrue(bs.test(9), "bit 9 set");
		assertTrue(not bs.test(5), "bit 5 not set");
		assertTrue(bs.any(), "bitset non-empty");
		assertTrue(bs.count() == 2, "count should be 2");

		bs.reset(0);
		assertTrue(not bs.test(0), "bit 0 cleared");
		assertTrue(bs.count() == 1, "count should be 1 after reset");

		bs.clearAll();
		assertTrue(not bs.any(), "clearAll empties the set");
	}

	void setOpsTest() {
		DynamicBitset a(8);
		DynamicBitset b(8);
		a.set(1);
		a.set(2);
		a.set(3);
		b.set(3);
		b.set(4);

		DynamicBitset uni = a;
		uni |= b;
		assertTrue(setBits(uni) == Indices{ 1, 2, 3, 4 }, "union should be {1,2,3,4}");

		DynamicBitset inter = a;
		inter &= b;
		assertTrue(setBits(inter) == Indices{ 3 }, "intersection should be {3}");

		DynamicBitset diff = a;
		diff -= b;
		assertTrue(setBits(diff) == Indices{ 1, 2 }, "difference should be {1,2}");

		// Operands are unchanged by the copies above.
		assertTrue(setBits(a) == Indices{ 1, 2, 3 }, "a unchanged");
		assertTrue(setBits(b) == Indices{ 3, 4 }, "b unchanged");
	}

	void equalityTest() {
		DynamicBitset a(16);
		DynamicBitset b(16);
		assertTrue(a == b, "two empty same-size bitsets are equal");
		a.set(7);
		assertTrue(not(a == b), "differ after a set");
		b.set(7);
		assertTrue(a == b, "equal again after same bit set");

		// Different capacity is never equal, even when both are empty.
		DynamicBitset small(8);
		DynamicBitset big(9);
		assertTrue(not(small == big), "different capacity bitsets are not equal");
	}

	void forEachSetTest() {
		DynamicBitset bs(200);
		for (usize i: Indices{ 0, 63, 64, 130, 199 }) bs.set(i);
		assertTrue(
			setBits(bs) == Indices{ 0, 63, 64, 130, 199 }, "iterates set bits in ascending order"
		);
		assertTrue(bs.count() == 5, "count matches number of set bits across words");
	}

	void largeCapacityTest() {
		// Exercise multiple words and the top bit of the last word.
		DynamicBitset bs(129);
		bs.set(128);
		assertTrue(bs.test(128), "top bit of a 3-word bitset is set");
		assertTrue(bs.count() == 1, "only one bit set");
		assertTrue(setBits(bs) == Indices{ 128 }, "only bit 128 iterated");
	}

	~DynamicBitsetTest() override = default;
};

TESTER_COMMON_MAIN("/src/base/tests/");
