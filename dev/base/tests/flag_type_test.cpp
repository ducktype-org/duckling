#include <tester/tester.hpp>
#include <base/flag.hpp>
#include <string>

class FlagTypeTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS FlagTypeTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(basicTest); }

	void basicTest() {
		using base::EmptyFlag;
		using base::FlagType;
		FlagType empty_flag;
		ASSERT_EQUAL(empty_flag, EmptyFlag);
		ASSERT_TRUE(!empty_flag.contains(FlagType(1)));

		FlagType flag_123 = FlagType(1) | FlagType(2) | FlagType(3);

		FlagType flag_123_oth;
		flag_123_oth |= FlagType(1);
		flag_123_oth |= FlagType(2);
		flag_123_oth |= FlagType(3);

		ASSERT_EQUAL(flag_123_oth, flag_123);

		ASSERT_TRUE(flag_123.contains(FlagType(1)));
		ASSERT_TRUE(flag_123.contains(FlagType(2)));
		ASSERT_TRUE(flag_123.contains(FlagType(3)));

		ASSERT_TRUE(flag_123.contains(FlagType(1) | FlagType(2)));
		ASSERT_TRUE(flag_123.contains(FlagType(1) | FlagType(3)));
		ASSERT_TRUE(flag_123.contains(FlagType(2) | FlagType(3)));

		ASSERT_TRUE(!flag_123.contains(FlagType(4)));
		ASSERT_TRUE(!flag_123.contains(FlagType(5)));
		ASSERT_TRUE(!flag_123.contains(FlagType(1) | FlagType(4)));
	}
};

TESTER_COMMON_MAIN("/base/tests/");
