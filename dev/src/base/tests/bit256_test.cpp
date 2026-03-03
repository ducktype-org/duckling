#include <base/types/bit256.hpp>

#include <tester/tester.hpp>

using namespace base::literals;

class Bit256Test: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS Bit256Test

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(constructorTest);
		TESTER_ADD_TEST(equalityTest);
		TESTER_ADD_TEST(toStringHexTest);
		TESTER_ADD_TEST(sumTest);
		TESTER_ADD_TEST(multiplicationTest);
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
			array_bit.data[0] == 0x00'00'00'00'FF'FF'FF'FF
				&& array_bit.data[1] == 0x9A'BC'DE'F0'12'34'56'78 && array_bit.data[2] == 0
				&& array_bit.data[3] == 0,
			"Array constructor failed"
		);

		base::Bit256 multi_arg_bit(0x1, 0x2, 0x3, 0x4);
		assertTrue(
			multi_arg_bit.data[0] == 0x1 && multi_arg_bit.data[1] == 0x2
				&& multi_arg_bit.data[2] == 0x3 && multi_arg_bit.data[3] == 0x4,
			"Multi-argument constructor failed"
		);

		base::Bit256 hex_bit
			= "0xFEDCBA9876543210AA77AA77AA77AA770123456789ABCDEFBB55BB55BB55BB55"_Bit256;
		assertTrue(
			hex_bit.data[3] == 0xFE'DC'BA'98'76'54'32'10
				&& hex_bit.data[2] == 0xAA'77'AA'77'AA'77'AA'77
				&& hex_bit.data[1] == 0x01'23'45'67'89'AB'CD'EF
				&& hex_bit.data[0] == 0xBB'55'BB'55'BB'55'BB'55,
			"Hex string constructor failed"
		);

		base::Bit256 hex_bit2 = "0x11111111111111112222222222222222"_Bit256;
		assertTrue(
			hex_bit2.data[3] == 0x0 && hex_bit2.data[2] == 0x0
				&& hex_bit2.data[1] == 0x11'11'11'11'11'11'11'11
				&& hex_bit2.data[0] == 0x22'22'22'22'22'22'22'22,
			"Hex string constructor failed for shorter string"
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
			bit.toStringHex() == "000000000000000000000000000000000fedcba987654321123456789abcdef0",
			"toStringHex failed. Expected:\n"
			"000000000000000000000000000000000fedcba987654321123456789abcdef0,\ngot:\n"
				+ bit.toStringHex()
		);

		base::Bit256 bit2
			= "0x603b5e62d77850a8a8592d066a263c893a9a8fca2c3af6d3e949f95e9c3f2905"_Bit256;
		assertTrue(
			bit2.toStringHex() == "603b5e62d77850a8a8592d066a263c893a9a8fca2c3af6d3e949f95e9c3f2905",
			"toStringHex failed. Expected:\n"
			"603b5e62d77850a8a8592d066a263c893a9a8fca2c3af6d3e949f95e9c3f2905,\ngot:\n"
				+ bit2.toStringHex()
		);
	}

	void sumTest() {
		base::Bit256 bit1(0xFF'FF'FF'FF'FF'FF'FF'FF, 0xFF'FF'FF'FF'FF'FF'FF'FF, 0x0, 0x0);
		base::Bit256 bit2(0x1, 0x0, 0x0, 0x0);
		bit1 += bit2;
		assertTrue(
			bit1.data[0] == 0 && bit1.data[1] == 0 && bit1.data[2] == 0x1 && bit1.data[3] == 0,
			"Addition operator failed #1\nExpected:\n{0, 0, 1, 0}\nGot:\n{"
				+ std::to_string(bit1.data[0]) + ", " + std::to_string(bit1.data[1]) + ", "
				+ std::to_string(bit1.data[2]) + ", " + std::to_string(bit1.data[3]) + "}"
		);

		base::Bit256 bit3
			= "0x603b5e62d77850a8a8592d066a263c893a9a8fca2c3af6d3e949f95e9c3f2905"_Bit256;
		base::Bit256 bit4
			= "0x9e80e21e00e871e0699c2a10e30f8ca9b58d3d35c3a0e5cdd113d3c45d620264"_Bit256;
		bit3 += bit4;
		assertTrue(
			bit3 == "0xfebc4080d860c28911f557174d35c932f027ccffefdbdca1ba5dcd22f9a12b69"_Bit256,
			"Addition operator failed #2"
		);
	}

	void multiplicationTest() {
		base::Bit256 bit1{ 0, 14'949'803'942'735'574'887ull };
		base::Bit256 bit2{ 14'327'648'566'732'417'020ull };
		base::Bit256 expected_1{
			0, 17'578'815'286'140'074'596ull, 11'611'563'329'397'473'609ull, 0
		};
		bit1 *= bit2;
		assertTrue(
			bit1 == expected_1,
			"Multiplication operator failed.\nExpected:\n{0, 17578815286140074596, "
			"11611563329397473609, 0}\n"
				+ expected_1.toStringHex() + "\nGot:\n{" + std::to_string(bit1.data[0]) + ", "
				+ std::to_string(bit1.data[1]) + ", " + std::to_string(bit1.data[2]) + ", "
				+ std::to_string(bit1.data[3]) + "}\n" + bit1.toStringHex()
		);

		base::Bit256 bit3       = "0xd49cb91d02e86314"_Bit256;
		base::Bit256 bit4       = "0x5657401d416a73b6"_Bit256;
		base::Bit256 expected_2 = "0x47b51cb222327b446338119390006c38"_Bit256;
		bit3 *= bit4;

		std::cerr << bit3 << '\n' << expected_2 << '\n';
		assertTrue(
			bit3 == expected_2,
			"Multiplication operator failed.\nExpected:\n" + expected_2.toStringHex() + "\nGot:\n"
				+ bit3.toStringHex()
		);

		base::Bit256 bit5
			= "0xd3dcad4eafc33c531c953fd294e375840007137468106e764982f31a6083a5f9"_Bit256;
		base::Bit256 bit6
			= "0x56a6a3f42517856499df6829c47b95efebc07cd86ba547f117397425218e231b"_Bit256;
		base::Bit256 expected_3
			= "0x7b539d9d359558f021db764b45387d3cf7058f2d9f0130add4cb916256b18c43"_Bit256;
		bit5 *= bit6;

		assertTrue(
			bit5 == expected_3,
			"Multiplication operator failed\nExpected:\n" + expected_3.toStringHex() + "\nGot:\n"
				+ bit5.toStringHex()
		);
	}

	~Bit256Test() override = default;
};

TESTER_COMMON_MAIN("/src/base/tests/");
