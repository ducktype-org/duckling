#include <base/flag.hpp>
#include <string>
#include <tester/tester.hpp>

class FlagTypeTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS FlagTypeTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("FlagType test") { TESTER_ADD_TEST(basicTest); }

	void basicTest() {
		using base::EmptyFlag;
		using base::FlagType;
		FlagType empty_flag;
		assert(empty_flag == EmptyFlag, "empty flag is not empty 1");
		assert(!empty_flag.contains(FlagType(1)), "empty flag is not empty 2");

		FlagType flag_123 = FlagType(1) | FlagType(2) | FlagType(3);

		FlagType flag_123_oth;
		flag_123_oth |= FlagType(1);
		flag_123_oth |= FlagType(2);
		flag_123_oth |= FlagType(3);

		assert(flag_123_oth == flag_123, "123 != 123");

		assert(flag_123.contains(FlagType(1)), "123 does not contains 1");
		assert(flag_123.contains(FlagType(2)), "123 does not contains 2");
		assert(flag_123.contains(FlagType(3)), "123 does not contains 3");

		assert(flag_123.contains(FlagType(1) | FlagType(2)), "123 does not contains 12");
		assert(flag_123.contains(FlagType(1) | FlagType(3)), "123 does not contains 13");
		assert(flag_123.contains(FlagType(2) | FlagType(3)), "123 does not contains 23");

		assert(!flag_123.contains(FlagType(4)), "123 contains 4");
		assert(!flag_123.contains(FlagType(5)), "123 contains 5");
		assert(!flag_123.contains(FlagType(1) | FlagType(4)), "123 contains 14");
	}
};

TESTER_COMMON_MAIN("/common/flag_type/tests/");
