// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/types/bit256.hpp>

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

		base::Bit256 hex_bit{
			"0xFeDcBA9876543210aa77AA77aa77AA770123456789ABCDEFbb55Bb55BB55Bb55"
		};
		assertTrue(
			hex_bit.data[3] == 0xFE'DC'BA'98'76'54'32'10
				&& hex_bit.data[2] == 0xAA'77'AA'77'AA'77'AA'77
				&& hex_bit.data[1] == 0x01'23'45'67'89'AB'CD'EF
				&& hex_bit.data[0] == 0xBB'55'BB'55'BB'55'BB'55,
			"Hex string constructor failed"
		);

		base::Bit256 hex_bit2("0x11111111111111112222222222222222");
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
			= base::Bit256{ "0x603b5E62d77850A8a8592d066a263C893A9a8fca2c3aF6d3e949f95e9C3f2905" };
		assertTrue(
			bit2.toStringHex() == "603b5e62d77850a8a8592d066a263c893a9a8fca2c3af6d3e949f95e9c3f2905",
			"toStringHex failed. Expected:\n"
			"603b5e62d77850a8a8592d066a263c893a9a8fca2c3af6d3e949f95e9c3f2905,\ngot:\n"
				+ bit2.toStringHex()
		);
	}

	~Bit256Test() override = default;
};

TESTER_COMMON_MAIN("/src/base/tests/");
