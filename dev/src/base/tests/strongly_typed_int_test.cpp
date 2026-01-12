#include <base/extend_cpp/strongly_typed_int.hpp>

#include <tester/tester.hpp>

STRONG_TYPEDEF_INT_DIMENSIONAL(Meters, i64);
STRONG_TYPEDEF_INT_DIMENSIONAL(TestU8, std::uint8_t);
STRONG_TYPEDEF_INT_DIMENSIONAL(TestU64, u64);

class StronglyTypedIntTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS StronglyTypedIntTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(stronglyTypedInt);
		TESTER_ADD_TEST(castingTest);
	}

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

	void castingTest() {
		Meters m(123);
		i64    as_i64 = static_cast<i64>(m);
		assertTrue(as_i64 == 123, "Cast failed 1");

		auto as_f64 = static_cast<double>(m);
		assertTrue(as_f64 == 123.0, "Cast failed 2");

		TestU8 small(255);
		auto   big = static_cast<TestU64>(small);
		assertTrue(static_cast<u64>(big) == 255, "Cast failed 3");

		TestU64 large(257);
		auto    smaller = static_cast<TestU8>(large);
		assertTrue(static_cast<uint8_t>(smaller) == 1, "Cast failed 4");
		assertTrue(static_cast<u8>(smaller) == u8(1), "Cast failed 5");

		u8  a(8);
		u64 b = static_cast<u64>(a);
		assertTrue(b == 8, "Cast failed 6");

		u8   no(0);
		u8   yes(1);
		bool x = static_cast<bool>(no);
		assertFalse(x, "Cast failed 7");
		bool y = static_cast<bool>(yes);
		assertTrue(y, "Cast failed 8");
	}

	~StronglyTypedIntTest() override = default;
};

TESTER_COMMON_MAIN("/src/base/tests/");
