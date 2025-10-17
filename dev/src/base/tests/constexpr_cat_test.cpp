#include <base/comptime/constexpr_cat.hpp>

#include <tester/tester.hpp>

class ConstexprCatTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ConstexprCatTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(testConstexprCat); }

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

	~ConstexprCatTest() override = default;

private:
};

TESTER_COMMON_MAIN("/src/base/tests/");
