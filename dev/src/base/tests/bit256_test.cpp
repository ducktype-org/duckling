#include <base/bit256.hpp>

#include <tester/tester.hpp>

class Bit256Test: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS Bit256Test

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(constructorTest);
		TESTER_ADD_TEST(equalityTest);
		TESTER_ADD_TEST(toStringHexTest);
	}

	void constructorTest() {
		base::Bit256 default_bit;
		assertTrue(
			default_bit.data[0] == 0 && default_bit.data[1] == 0 && default_bit.data[2] == 0
				&& default_bit.data[3] == 0,
			"Default constructor failed"
		);

		std::array<u32, 8> arr
			= { 0xFF'FF'FF'FF, 0x00'00'00'00, 0x12'34'56'78, 0x9A'BC'DE'F0, 0x0, 0x0, 0x0, 0x0 };
		base::Bit256 array_bit(arr);
		assertTrue(
			array_bit.data[0] == 0xFF'FF'FF'FF'00'00'00'00
				&& array_bit.data[1] == 0x12'34'56'78'9A'BC'DE'F0 && array_bit.data[2] == 0
				&& array_bit.data[3] == 0,
			"Array constructor failed"
		);

		base::Bit256 multi_arg_bit(0x1, 0x2, 0x3, 0x4);
		assertTrue(
			multi_arg_bit.data[0] == 0x1 && multi_arg_bit.data[1] == 0x2
				&& multi_arg_bit.data[2] == 0x3 && multi_arg_bit.data[3] == 0x4,
			"Multi-argument constructor failed"
		);
	}

	void equalityTest() {
		base::Bit256 bit1(0x1, 0x2, 0x3, 0x4);
		base::Bit256 bit2(0x1, 0x2, 0x3, 0x4);
		base::Bit256 bit3(0x5, 0x6, 0x7, 0x8);

		assertTrue(bit1 == bit2, "Equality operator failed");
		assertTrue(bit1 != bit3, "Inequality operator failed");
	}

	void toStringHexTest() {
		base::Bit256 bit(0x12'34'56'78'9A'BC'DE'F0, 0x0F'ED'CB'A9'87'65'43'21, 0x0, 0x0);
		assertTrue(
			bit.toStringHex() == "123456789abcdef00fedcba98765432100000000000000000000000000000000",
			"toStringHex failed"
		);
	}

	~Bit256Test() override = default;
};

TESTER_COMMON_MAIN("/src/base/tests/");
