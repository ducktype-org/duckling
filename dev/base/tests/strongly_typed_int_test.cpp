#include <tester/tester.hpp>
#include <tester/testing_utils.hpp>
#include <base/strongly_typed_int.hpp>

STRONG_TYPEDEF_INT_DIMENSIONAL(Meters, i64);

class StronglyTypedIntTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS StronglyTypedIntTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Strongly typed int Test") {
		TESTER_ADD_TEST(stronglyTypedInt);
	}

	void stronglyTypedInt() {
		Meters m(0);
		assert(i64(m) == 0, "Basic math failed (1)");
		assert(m == Meters(0), "Basic math failed (2)");

		Meters m1(2);
		Meters m2 = m + m1;
		assert(m2 == Meters(2), "Basic math failed (3)");
		assert(-m2 == Meters(-2), "Basic math failed (4)");
		assert(+(-m2) == Meters(-2), "Basic math failed (5)");
		assert(m2 != Meters(1), "Basic math failed (6)");
		assert(m2 > Meters(0), "Basic math failed (7)");
		assert(m2 > Meters(1), "Basic math failed (8)");
		assert(m2 >= Meters(2), "Basic math failed (9)");
		assert(m2 <= Meters(2), "Basic math failed (10)");
		assert(m2 < Meters(3), "Basic math failed (11)");
		assert(m2 < Meters(10), "Basic math failed (12)");

		assert(m2 - Meters(10) == Meters(-8), "Basic math failed (13)");
		assert(m2 * 2 == Meters(4), "Basic math failed (14)");
		assert(m2 / 2 == Meters(1), "Basic math failed (15)");

		m2 += Meters(10);
		assert(m2 == Meters(12), "Basic math failed (16)");
		m2 -= Meters(20);
		assert(m2 == Meters(-8), "Basic math failed (17)");
	}

	~StronglyTypedIntTest() override = default;
};

TESTER_COMMON_MAIN("/base/tests/");
