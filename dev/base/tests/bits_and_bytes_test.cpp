#include <tester/tester.hpp>
#include <tester/testing_utils.hpp>
#include <base/bits_and_bytes.hpp>

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
		assertTrue(-n3 == B(-3), "Basic math failed (4)");
		assertTrue(+(-n3) == B(-3), "Basic math failed (5)");
		assertTrue(n3 != B(1), "Basic math failed (6)");
		assertTrue(n3 > B(1), "Basic math failed (7)");
		assertTrue(n3 > B(2), "Basic math failed (8)");
		assertTrue(n3 >= B(3), "Basic math failed (9)");
		assertTrue(n3 <= B(3), "Basic math failed (10)");
		assertTrue(n3 < B(4), "Basic math failed (11)");
		assertTrue(n3 < B(10), "Basic math failed (12)");

		assertTrue(n3 - B(10) == B(-7), "Basic math failed (13)");
		assertTrue(n3 * 2 == B(6), "Basic math failed (14)");
		assertTrue(n3 / 2 == B(1), "Basic math failed (15)");

		n3 += B(10);
		assertTrue(n3 == B(13), "Basic math failed (16)");
		n3 -= B(20);
		assertTrue(n3 == B(-7), "Basic math failed (17)");
	}

	void bitsAndBytesTest() {
		// Basic maffs.
		unitTest<Bits>();
		unitTest<Bytes>();

		// Additional functionalities test.
		Bytes Bs = Bytes(3);
		Bits  bs = bytes2bits(Bs);
		assertTrue(usize(bs) == 3 * 8, "Conversion failed");

		assertTrue(std::to_string(Bs) == "3B", "Byte stringification failed");
		assertTrue(std::to_string(bs) == "24b", "Bit stringification failed");
	}

	~BitsAndBytesTest() override = default;
};

TESTER_COMMON_MAIN("/base/tests/");
