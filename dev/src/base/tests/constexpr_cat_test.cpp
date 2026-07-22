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
		constexpr std::array res1 = CONSTEXPR_CAT("aBd", "Inny string");
		std::string_view     view = { res1.data(), res1.size() };

		ASSERT_EQUAL(view, "aBdInny string");

		constexpr std::array res2 = CONSTEXPR_CAT('x', "aBd", "Inny string", "", 'u', "aaa");
		view                      = { res2.data(), res2.size() };

		ASSERT_EQUAL(view, "xaBdInny stringuaaa");

		constexpr std::array res3 = CONSTEXPR_CAT(res1, res2, 'x');
		view                      = { res3.data(), res3.size() };

		assertTrue(
			view == "aBdInny stringxaBdInny stringuaaax",
			"CONSTEXPR_CAT returned answer other than expected"
		);
	}

	/**
	 * @brief Regression test for #2258: CONSTEXPR_CAT output used as C-string
	 *        requires explicit null termination.
	 *
	 * CONSTEXPR_CAT strips the trailing \\0 from string literals. Arrays
	 * produced without an explicit '\\0' argument are NOT safe for .data()
	 * C-string usage. This test verifies both behaviors.
	 */
	void testNullTerminatorBehavior() {
		// Without explicit '\\0': NO null terminator.
		constexpr std::array no_null = CONSTEXPR_CAT("Paused");
		ASSERT_EQUAL(no_null.size(), 6ULL);
		ASSERT_EQUAL(no_null[5], 'd');  // Last char is 'd', not '\0'.

		// With explicit '\\0': null-terminated, safe for .data() C-string.
		constexpr std::array with_null = CONSTEXPR_CAT("Paused", '\0');
		ASSERT_EQUAL(with_null.size(), 7ULL);
		ASSERT_EQUAL(with_null[6], '\0');

		// C-string comparison confirms null-terminated output.
		std::string_view as_cstr(with_null.data());
		ASSERT_EQUAL(as_cstr, "Paused");
	}

	/**
	 * @brief Verifies CONSTEXPR_CAT_CSTR produces null-terminated output
	 *        without requiring manual '\\0' in the argument list.
	 */
	void testConstexprCatCstr() {
		// Single argument.
		constexpr std::array a = CONSTEXPR_CAT_CSTR("Paused");
		ASSERT_EQUAL(a.size(), 7ULL);
		ASSERT_EQUAL(a[6], '\0');
		ASSERT_EQUAL(std::string_view(a.data()), "Paused");

		// Multiple arguments.
		constexpr std::array b = CONSTEXPR_CAT_CSTR("Hello", ", ", "world");
		ASSERT_EQUAL(b.size(), 13ULL);
		ASSERT_EQUAL(b[12], '\0');
		ASSERT_EQUAL(std::string_view(b.data()), "Hello, world");

		// Works with char arguments.
		constexpr std::array c = CONSTEXPR_CAT_CSTR("a", 'b', "c");
		ASSERT_EQUAL(c.size(), 4ULL);
		ASSERT_EQUAL(c[3], '\0');
		ASSERT_EQUAL(std::string_view(c.data()), "abc");
	}

	~ConstexprCatTest() override = default;

private:
};

TESTER_COMMON_MAIN("/src/base/tests/");
