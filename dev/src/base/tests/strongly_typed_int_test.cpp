#include <base/strongly_typed_int.hpp>

#include <tester/tester.hpp>
#include <tester/testing_utils.hpp>

STRONG_TYPEDEF_INT_DIMENSIONAL(Meters, i64);

class StronglyTypedIntTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS StronglyTypedIntTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(stronglyTypedInt); }

	void stronglyTypedInt() {
		Meters m(0);
		assertTrue(i64(m) == 0, "Basic math failed (1)");
		assertTrue(m == Meters(0), "Basic math failed (2)");

		Meters m1(2);
		Meters m2 = m + m1;
		assertTrue(m2 == Meters(2), "Basic math failed (3)");
		assertTrue(-m2 == Meters(-2), "Basic math failed (4)");
		assertTrue(+(-m2) == Meters(-2), "Basic math failed (5)");
		assertTrue(m2 != Meters(1), "Basic math failed (6)");
		assertTrue(m2 > Meters(0), "Basic math failed (7)");
		assertTrue(m2 > Meters(1), "Basic math failed (8)");
		assertTrue(m2 >= Meters(2), "Basic math failed (9)");
		assertTrue(m2 <= Meters(2), "Basic math failed (10)");
		assertTrue(m2 < Meters(3), "Basic math failed (11)");
		assertTrue(m2 < Meters(10), "Basic math failed (12)");

		assertTrue(m2 - Meters(10) == Meters(-8), "Basic math failed (13)");
		assertTrue(m2 * 2 == Meters(4), "Basic math failed (14)");
		assertTrue(m2 / 2 == Meters(1), "Basic math failed (15)");

		m2 += Meters(10);
		assertTrue(m2 == Meters(12), "Basic math failed (16)");
		m2 -= Meters(20);
		assertTrue(m2 == Meters(-8), "Basic math failed (17)");
	}

	~StronglyTypedIntTest() override = default;
};

TESTER_COMMON_MAIN("/src/base/tests/");
