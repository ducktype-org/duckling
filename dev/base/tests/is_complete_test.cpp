#include <tester/tester.hpp>
#include <base/is_complete.hpp>

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
		if constexpr (IS_COMPLETE_V<IncompleteType>) fail("IncompleteType should be incomplete");
		if constexpr (IS_COMPLETE_V<struct InlineIncompleteType>)
			fail("InlineIncompleteType should be incomplete");
		if constexpr (!IS_COMPLETE_V<CompleteType>) fail("CompleteType should be complete");
	}
};

TESTER_COMMON_MAIN("/base/tests/");
