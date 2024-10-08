#include <tester/tester.hpp>
#include <filesystem/file.hpp>
#include <base/str_utils.hpp>

class ConcatTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ConcatTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(testStrConcat); }

	void testStrConcat() {
		std::string res;

		res = base::strConcat("aBd", 1, 5, true, "Inny string");
		assertTrue(res == "aBd15trueInny string", "strConcat returned answer other than expected");

		res = base::strConcat();
		assertTrue(res == "", "strConcat returned answer other than expected");

		res = base::strConcat("", "", "");
		assertTrue(res == "", "strConcat returned answer other than expected");

		res = base::strConcat(1, -87, 123'456'789ull);
		assertTrue(res == "1-87123456789", "strConcat returned answer other than expected");

		assertThrows<std::domain_error>(
			[]() { base::strConcat("abacabadaba", nullptr); },
			"strConcat of nullptr did not throw correctly"
		);

		assertThrows<std::domain_error>(
			[]() {
				char* ptr = nullptr;
				base::strConcat("abacabadaba", ptr);
			},
			"strConcat of nullptr did not throw correctly"
		);
	}

	~ConcatTest() override = default;

private:
};

TESTER_COMMON_MAIN("/base/tests/");
