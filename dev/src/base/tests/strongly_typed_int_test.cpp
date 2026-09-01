#include <base/extend_cpp/strongly_typed_int.hpp>

#include <tester/tester.hpp>

STRONG_TYPEDEF_INT_DIMENSIONAL(Meters, i64);
STRONG_TYPEDEF_INT_DIMENSIONAL(TestU8, std::uint8_t);
STRONG_TYPEDEF_INT_DIMENSIONAL(TestU64, u64);
STRONG_TYPEDEF_INT_BINARY(TestBits, std::uint8_t);

class StronglyTypedIntTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS StronglyTypedIntTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(stronglyTypedInt);
		TESTER_ADD_TEST(castingTest);
		TESTER_ADD_TEST(bitwiseTest);
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

	void bitwiseTest() {
		const TestBits a(0b1100);
		const TestBits b(0b1010);

		assertTrue((a & b) == TestBits(0b1000), "Bitwise failed (1)");
		assertTrue((a | b) == TestBits(0b1110), "Bitwise failed (2)");
		assertTrue((a ^ b) == TestBits(0b0110), "Bitwise failed (3)");
		assertTrue(~TestBits(0) == TestBits(0xFF), "Bitwise failed (4)");

		assertTrue((TestBits(1) << TestBits(3)) == TestBits(8), "Bitwise failed (5)");
		assertTrue((TestBits(1) << 3) == TestBits(8), "Bitwise failed (6)");
		assertTrue((TestBits(8) >> TestBits(2)) == TestBits(2), "Bitwise failed (7)");
		assertTrue((TestBits(8) >> 2) == TestBits(2), "Bitwise failed (8)");

		TestBits c(0b0011);
		c |= TestBits(0b0100);
		assertTrue(c == TestBits(0b0111), "Bitwise failed (9)");
		c &= TestBits(0b1110);
		assertTrue(c == TestBits(0b0110), "Bitwise failed (10)");
		c ^= TestBits(0b0011);
		assertTrue(c == TestBits(0b0101), "Bitwise failed (11)");
		c <<= 1;
		assertTrue(c == TestBits(0b1010), "Bitwise failed (12)");
		c >>= TestBits(1);
		assertTrue(c == TestBits(0b0101), "Bitwise failed (13)");
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
