#include <base/types/ok_bad.hpp>

#include <tester/tester.hpp>

class OkBadTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS OkBadTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(basicTest); }

	void basicTest() {
		static_assert(sizeof(base::OkBad) == sizeof(bool), "OkBad should be the same size as bool");

		ASSERT_TRUE(base::OK.isOk());
		ASSERT_TRUE(base::BAD.isBad());

		ASSERT_TRUE(not base::OK.isBad());
		ASSERT_TRUE(not base::BAD.isOk());
	}
};

TESTER_COMMON_MAIN("/src/base/tests/");
