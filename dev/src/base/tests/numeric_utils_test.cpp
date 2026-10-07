// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/numeric/numeric_utils.hpp>
#include <base/types/ints.hpp>

#include <tester/tester.hpp>

#include <limits>

class NumericUtilsTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS NumericUtilsTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testSignedToSigned);
		TESTER_ADD_TEST(testUnsignedToUnsigned);
		TESTER_ADD_TEST(testSignedToUnsigned);
		TESTER_ADD_TEST(testUnsignedToSigned);
	}

private:
	void testSignedToSigned() {
		static_assert(base::fitsIn<i64, i16>(0), "0 should fit from i16 to i64");
		static_assert(
			base::fitsIn<i64, i16>(std::numeric_limits<i16>::max()), "max i16 should fit in i64"
		);
		static_assert(
			base::fitsIn<i64, i16>(std::numeric_limits<i16>::min()), "min i16 should fit in i64"
		);

		static_assert(base::fitsIn<i16, i64>(100), "100 should fit from i64 to i16");
		static_assert(base::fitsIn<i16, i64>(-100), "-100 should fit from i64 to i16");
		static_assert(
			base::fitsIn<i16, i64>(std::numeric_limits<i16>::max()),
			"max i16 value should fit from i64 to i16"
		);
		static_assert(
			base::fitsIn<i16, i64>(std::numeric_limits<i16>::min()),
			"min i16 value should fit from i64 to i16"
		);

		static_assert(
			!base::fitsIn<i16, i64>(static_cast<i64>(std::numeric_limits<i16>::max()) + 1),
			"max i16 + 1 should not fit in i16 (overflow)"
		);
		static_assert(
			!base::fitsIn<i16, i64>(static_cast<i64>(std::numeric_limits<i16>::min()) - 1),
			"min i16 - 1 should not fit in i16 (underflow)"
		);

		static_assert(
			base::fitsIn<i32, i32>(std::numeric_limits<i32>::max()), "max i32 should fit in i32"
		);
		static_assert(
			base::fitsIn<i32, i32>(std::numeric_limits<i32>::min()), "min i32 should fit in i32"
		);
	}

	void testUnsignedToUnsigned() {
		static_assert(base::fitsIn<u64, u16>(0), "0 should fit from u16 to u64");
		static_assert(
			base::fitsIn<u64, u16>(std::numeric_limits<u16>::max()), "max u16 should fit in u64"
		);
		static_assert(base::fitsIn<u16, u64>(100), "100 should fit from u64 to u16");
		static_assert(
			base::fitsIn<u16, u64>(std::numeric_limits<u16>::max()),
			"max u16 value should fit from u64 to u16"
		);
		static_assert(
			!base::fitsIn<u16, u64>(static_cast<u64>(std::numeric_limits<u16>::max()) + 1),
			"max u16 + 1 should not fit in u16 (overflow)"
		);
		static_assert(
			base::fitsIn<u32, u32>(std::numeric_limits<u32>::max()), "max u32 should fit in u32"
		);
	}

	void testSignedToUnsigned() {
		static_assert(!base::fitsIn<u32, i32>(-1), "Negative value should not fit in unsigned");
		static_assert(
			!base::fitsIn<u64, i32>(std::numeric_limits<i32>::min()),
			"Min signed value should not fit in unsigned"
		);
		static_assert(base::fitsIn<u16, i32>(100), "100 should fit from i32 to u16");
		static_assert(
			base::fitsIn<u16, i32>(std::numeric_limits<u16>::max()),
			"max u16 value should fit from i32 to u16"
		);
		static_assert(
			!base::fitsIn<u16, i32>(static_cast<i32>(std::numeric_limits<u16>::max()) + 1),
			"max u16 + 1 should not fit from i32 to u16"
		);

		static_assert(base::fitsIn<u32, i16>(0), "0 should fit from i16 to u32");
		static_assert(
			base::fitsIn<u32, i16>(std::numeric_limits<i16>::max()), "max i16 should fit in u32"
		);

		static_assert(
			base::fitsIn<u32, i32>(std::numeric_limits<i32>::max()), "max i32 should fit in u32"
		);
	}

	void testUnsignedToSigned() {
		static_assert(base::fitsIn<i32, u16>(0), "0 should fit from u16 to i32");
		static_assert(
			base::fitsIn<i32, u16>(std::numeric_limits<u16>::max()), "max u16 should fit in i32"
		);
		static_assert(
			base::fitsIn<i64, u32>(std::numeric_limits<u32>::max()), "max u32 should fit in i64"
		);
		static_assert(base::fitsIn<i32, u32>(100), "100 should fit from u32 to i32");
		static_assert(
			base::fitsIn<i32, u32>(std::numeric_limits<i32>::max()),
			"max i32 value should fit from u32 to i32"
		);
		static_assert(
			!base::fitsIn<i32, u32>(static_cast<u32>(std::numeric_limits<i32>::max()) + 1),
			"max i32 + 1 should not fit from u32 to i32"
		);
		static_assert(
			!base::fitsIn<i32, u32>(std::numeric_limits<u32>::max()), "max u32 should not fit in i32"
		);
		static_assert(
			!base::fitsIn<i32, u64>(std::numeric_limits<u64>::max()), "max u64 should not fit in i32"
		);
	}
};

TESTER_COMMON_MAIN("/src/base/tests/");
