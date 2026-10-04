#include <base/comptime/is_complete.hpp>

#include <tester/tester.hpp>

namespace {
	struct IncompleteType;

	struct CompleteType {};
}

class IsCompleteTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS IsCompleteTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(basicTest); }

	void basicTest() {
		static_assert(!IS_COMPLETE_V<IncompleteType>, "IncompleteType should be incomplete");
		static_assert(
			!IS_COMPLETE_V<struct InlineIncompleteType>, "InlineIncompleteType should be incomplete"
		);
		static_assert(IS_COMPLETE_V<CompleteType>, "CompleteType should be complete");
	}
};

TESTER_COMMON_MAIN("/src/base/tests/");
