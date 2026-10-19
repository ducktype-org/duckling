#include <base/misc/no_unique_address.hpp>

#include <tester/tester.hpp>

namespace {
	struct Empty {};

	struct Holder {
		int                     value;
		NO_UNIQUE_ADDRESS Empty empty;
	};
}

class NoUniqueAddressTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS NoUniqueAddressTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(emptyMemberTakesNoSpaceTest); }

	// On MSVC and clang-cl the standard spelling is accepted and ignored, so this is what
	// tells the two apart.
	void emptyMemberTakesNoSpaceTest() { static_assert(sizeof(Holder) == sizeof(int)); }
};

TESTER_COMMON_MAIN("/src/base/tests/");
