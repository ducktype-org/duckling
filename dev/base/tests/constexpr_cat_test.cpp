#include <tester/tester.hpp>
#include <filesystem/file.hpp>
#include <base/constexpr_cat.hpp>

class ConstexprCatTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ConstexprCatTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Constexpr cat Test") { TESTER_ADD_TEST(testConstexprCat); }

	void testConstexprCat() {
		constexpr std::array res1 = CONSTEXPR_CAT("aBd", "Inny string");
		std::string_view     view = { res1.data(), res1.size() };

		assert(view == "aBdInny string", "CONSTEXPR_CAT returned answer other than expected");

		constexpr std::array res2 = CONSTEXPR_CAT('x', "aBd", "Inny string", "", 'u', "aaa");
		view                      = { res2.data(), res2.size() };

		assert(view == "xaBdInny stringuaaa", "CONSTEXPR_CAT returned answer other than expected");

		constexpr std::array res3 = CONSTEXPR_CAT(res1, res2, 'x');
		view                      = { res3.data(), res3.size() };

		assert(
			view == "aBdInny stringxaBdInny stringuaaax",
			"CONSTEXPR_CAT returned answer other than expected"
		);
	}

	~ConstexprCatTest() override = default;

private:
};

TESTER_COMMON_MAIN("base/tests/");
