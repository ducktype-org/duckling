// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/comptime/constexpr_cat.hpp>

#include <tester/tester.hpp>

class ConstexprCatTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ConstexprCatTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testConstexprCat);
		TESTER_ADD_TEST(testNullTerminatorBehavior);
		TESTER_ADD_TEST(testConstexprCatCstr);
	}

	void testConstexprCat() {
		constexpr std::array RES1 = CONSTEXPR_CAT("A", "B");
		std::string_view     view = { RES1.data(), RES1.size() };

		ASSERT_EQUAL_PRINT(view, "AB");

		constexpr std::array RES2 = CONSTEXPR_CAT('A', "B", "C", "", 'd', "eee");
		view                      = { RES2.data(), RES2.size() };

		ASSERT_EQUAL_PRINT(view, "ABCdeee");

		constexpr std::array RES3 = CONSTEXPR_CAT(RES1, RES2, 'x');
		view                      = { RES3.data(), RES3.size() };

		ASSERT_EQUAL_PRINT(view, "ABABCdeeex");
	}

	/**
	 * @brief CONSTEXPR_CAT output used as C-string
	 * 		requires explicit null termination.
	 *
	 * CONSTEXPR_CAT strips the trailing \\0 from string literals. Arrays
	 * produced without an explicit '\\0' argument are NOT safe for .data()
	 * C-string usage. This test verifies both behaviors.
	 */
	void testNullTerminatorBehavior() {
		// Without explicit '\\0': NO null terminator.
		constexpr std::array NO_NULL = CONSTEXPR_CAT("Paused");
		ASSERT_EQUAL(NO_NULL.size(), 6ULL);
		ASSERT_EQUAL(NO_NULL[5], 'd');

		// With explicit '\\0': null-terminated, safe for .data() C-string.
		constexpr std::array WITH_NULL = CONSTEXPR_CAT("Paused", '\0');
		ASSERT_EQUAL(WITH_NULL.size(), 7ULL);
		ASSERT_EQUAL(WITH_NULL[6], '\0');

		// C-string comparison confirms null-terminated output.
		std::string_view as_cstr(WITH_NULL.data());
		ASSERT_EQUAL(as_cstr, "Paused");
	}

	/**
	 * @brief Verifies CONSTEXPR_CAT_CSTR produces null-terminated output
	 *        without requiring manual '\\0' in the argument list.
	 */
	void testConstexprCatCstr() {
		// Single argument.
		constexpr std::array SINGLE_ARG = CONSTEXPR_CAT_CSTR("Paused");
		ASSERT_EQUAL(SINGLE_ARG.size(), 7ULL);
		ASSERT_EQUAL(SINGLE_ARG[6], '\0');
		ASSERT_EQUAL(std::string_view(SINGLE_ARG.data()), "Paused");

		// Multiple arguments.
		constexpr std::array MULTI_ARG = CONSTEXPR_CAT_CSTR("Hello", ", ", "world");
		ASSERT_EQUAL(MULTI_ARG.size(), 13ULL);
		ASSERT_EQUAL(MULTI_ARG[12], '\0');
		ASSERT_EQUAL(std::string_view(MULTI_ARG.data()), "Hello, world");

		// Works with char arguments.
		constexpr std::array WITH_CHAR = CONSTEXPR_CAT_CSTR("a", 'b', "c");
		ASSERT_EQUAL(WITH_CHAR.size(), 4ULL);
		ASSERT_EQUAL(WITH_CHAR[3], '\0');
		ASSERT_EQUAL(std::string_view(WITH_CHAR.data()), "abc");
	}

	~ConstexprCatTest() override = default;

private:
};

TESTER_COMMON_MAIN("/src/base/tests/");
